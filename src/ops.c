#include "ops.h"
#include "ndarray.h"
#include "nditer.h"

#include "Zend/zend_exceptions.h"

#include <math.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <float.h>

/* ============================================================================
 * Output-dtype rules (matching the sprint plan in docs/sprints/)
 * ----------------------------------------------------------------------------
 *   sum:    int → int64; f32 → f32; f64 → f64
 *   mean:   int → f64;   f32 → f32; f64 → f64
 *   min/max:                 preserve input dtype
 *   var/std: int → f64;  f32 → f32; f64 → f64  (compute in double, cast)
 *   argmin/argmax:        int64 always
 * ============================================================================ */

static numphp_dtype reduce_out_dtype(numphp_reduce_op op, numphp_dtype in)
{
    switch (op) {
        case NUMPHP_REDUCE_SUM:
            if (in == NUMPHP_INT32 || in == NUMPHP_INT64 || in == NUMPHP_BOOL) return NUMPHP_INT64;
            return in;
        case NUMPHP_REDUCE_MEAN:
        case NUMPHP_REDUCE_VAR:
        case NUMPHP_REDUCE_STD:
            if (in == NUMPHP_INT32 || in == NUMPHP_INT64 || in == NUMPHP_BOOL) return NUMPHP_FLOAT64;
            return in;
        case NUMPHP_REDUCE_MIN:
        case NUMPHP_REDUCE_MAX:
            return in;
        case NUMPHP_REDUCE_ARGMIN:
        case NUMPHP_REDUCE_ARGMAX:
            return NUMPHP_INT64;
    }
    return in;
}

/* ----------------------------------------------------------------------------
 * Pairwise sum — recursively halves to limit floating-point roundoff.
 * Threshold matches NumPy's general approach (small leaf ~ 8 elements).
 * Used for the f32 / f64 sum and as part of mean.
 * -------------------------------------------------------------------------- */
static double pairwise_sum_f64(const char *p, zend_long stride, zend_long n, numphp_dtype dt)
{
    if (n <= 8) {
        double s = 0.0;
        for (zend_long i = 0; i < n; i++) {
            s += numphp_read_f64(p + i * stride, dt);
        }
        return s;
    }
    zend_long m = n / 2;
    return pairwise_sum_f64(p, stride, m, dt)
         + pairwise_sum_f64(p + m * stride, stride, n - m, dt);
}

static double pairwise_sum_f64_skipnan(const char *p, zend_long stride, zend_long n,
                                       numphp_dtype dt, zend_long *count_out)
{
    if (n <= 8) {
        double s = 0.0;
        zend_long c = 0;
        for (zend_long i = 0; i < n; i++) {
            double v = numphp_read_f64(p + i * stride, dt);
            if (isnan(v)) continue;
            s += v;
            c++;
        }
        *count_out = c;
        return s;
    }
    zend_long m = n / 2;
    zend_long c1 = 0, c2 = 0;
    double s1 = pairwise_sum_f64_skipnan(p, stride, m, dt, &c1);
    double s2 = pairwise_sum_f64_skipnan(p + m * stride, stride, n - m, dt, &c2);
    *count_out = c1 + c2;
    return s1 + s2;
}

/* ----------------------------------------------------------------------------
 * Axis-0 sum tiled kernel — 2-D C-contiguous f32/f64 only.
 *
 * Strategy: process columns in strips of NUMPHP_AXIS0_TILE. For each strip
 * we run a pairwise recursion on rows that keeps `strip_w` accumulators in
 * lockstep, so a single row read at the leaf serves `strip_w` columns.
 * The recursion structure on rows is identical to the slow path's
 * pairwise_sum_f64, so each output cell is bit-identical to the
 * pre-optimisation value.
 *
 * Speedup vs the slow path comes from:
 *   - Reads at the leaf hit consecutive cache lines (contiguous along
 *     stride-1) instead of one cache miss per row (40 KB stride for a
 *     5000-wide f64 array).
 *   - No per-element function-call dispatch; the dtype is known.
 *   - Compiler is free to vectorise the leaf's inner per-strip loop.
 * -------------------------------------------------------------------------- */

#define NUMPHP_AXIS0_TILE 32

static void pairwise_sum_strip_f64(const double * __restrict__ src, zend_long nrows,
                                   zend_long row_stride_doubles,
                                   int strip_w, double * __restrict__ acc)
{
    if (nrows <= 8) {
        if (strip_w == NUMPHP_AXIS0_TILE) {
            /* Hot path: full-tile leaf. Fixed strip_w lets the compiler
             * unroll and vectorise the inner add. */
            for (zend_long r = 0; r < nrows; r++) {
                const double * __restrict__ row = src + r * row_stride_doubles;
                for (int w = 0; w < NUMPHP_AXIS0_TILE; w++) acc[w] += row[w];
            }
        } else {
            for (zend_long r = 0; r < nrows; r++) {
                const double * __restrict__ row = src + r * row_stride_doubles;
                for (int w = 0; w < strip_w; w++) acc[w] += row[w];
            }
        }
        return;
    }
    zend_long m = nrows / 2;
    double tmp[NUMPHP_AXIS0_TILE] = {0};
    pairwise_sum_strip_f64(src, m, row_stride_doubles, strip_w, tmp);
    pairwise_sum_strip_f64(src + m * row_stride_doubles, nrows - m,
                           row_stride_doubles, strip_w, acc);
    for (int w = 0; w < strip_w; w++) acc[w] += tmp[w];
}

static void axis0_sum_f64_2d(const double *src, zend_long nrows, zend_long ncols,
                             double *out)
{
    memset(out, 0, (size_t)ncols * sizeof(double));
    for (zend_long c0 = 0; c0 < ncols; c0 += NUMPHP_AXIS0_TILE) {
        int w = (int)((ncols - c0 < NUMPHP_AXIS0_TILE)
                       ? (ncols - c0)
                       : NUMPHP_AXIS0_TILE);
        pairwise_sum_strip_f64(src + c0, nrows, ncols, w, out + c0);
    }
}

/* f32 variant: accumulate in f64 to match the slow path's precision contract
 * (pairwise_sum_f64 always reads-and-promotes-to-double internally), then
 * cast to f32 on write. */
static void pairwise_sum_strip_f32_to_f64(const float * __restrict__ src, zend_long nrows,
                                          zend_long row_stride_floats,
                                          int strip_w, double * __restrict__ acc)
{
    if (nrows <= 8) {
        if (strip_w == NUMPHP_AXIS0_TILE) {
            for (zend_long r = 0; r < nrows; r++) {
                const float * __restrict__ row = src + r * row_stride_floats;
                for (int w = 0; w < NUMPHP_AXIS0_TILE; w++) acc[w] += (double)row[w];
            }
        } else {
            for (zend_long r = 0; r < nrows; r++) {
                const float * __restrict__ row = src + r * row_stride_floats;
                for (int w = 0; w < strip_w; w++) acc[w] += (double)row[w];
            }
        }
        return;
    }
    zend_long m = nrows / 2;
    double tmp[NUMPHP_AXIS0_TILE] = {0};
    pairwise_sum_strip_f32_to_f64(src, m, row_stride_floats, strip_w, tmp);
    pairwise_sum_strip_f32_to_f64(src + m * row_stride_floats, nrows - m,
                                  row_stride_floats, strip_w, acc);
    for (int w = 0; w < strip_w; w++) acc[w] += tmp[w];
}

static void axis0_sum_f32_2d(const float *src, zend_long nrows, zend_long ncols,
                             float *out)
{
    double *acc = (double *)ecalloc((size_t)ncols, sizeof(double));
    for (zend_long c0 = 0; c0 < ncols; c0 += NUMPHP_AXIS0_TILE) {
        int w = (int)((ncols - c0 < NUMPHP_AXIS0_TILE)
                       ? (ncols - c0)
                       : NUMPHP_AXIS0_TILE);
        pairwise_sum_strip_f32_to_f64(src + c0, nrows, ncols, w, acc + c0);
    }
    for (zend_long c = 0; c < ncols; c++) out[c] = (float)acc[c];
    efree(acc);
}

/* ----------------------------------------------------------------------------
 * Welford's online algorithm — single pass, numerically stable variance.
 *
 *   M2 = 0, mean = 0, n = 0
 *   for x:
 *     n   += 1
 *     d   = x - mean
 *     mean += d / n
 *     d2  = x - mean
 *     M2  += d * d2
 *   var = M2 / (n - ddof)
 * -------------------------------------------------------------------------- */
static int welford_axis(const char *p, zend_long stride, zend_long n,
                        numphp_dtype dt, int skip_nan,
                        double *mean_out, double *m2_out, zend_long *count_out)
{
    double mean = 0.0, m2 = 0.0;
    zend_long count = 0;
    for (zend_long i = 0; i < n; i++) {
        double x = numphp_read_f64(p + i * stride, dt);
        if (skip_nan && isnan(x)) continue;
        count++;
        double d = x - mean;
        mean += d / (double)count;
        double d2 = x - mean;
        m2 += d * d2;
    }
    *mean_out = mean;
    *m2_out = m2;
    *count_out = count;
    return 1;
}

/* ----------------------------------------------------------------------------
 * Reduce a single 1-D strided "line" of values into one scalar. Used both for
 * global reductions (over the contiguous flattened array) and for axis
 * reductions (one call per outer index).
 *
 * Writes the result into `out_ptr` using `out_dt`. For ARGMIN/ARGMAX, writes
 * an int64 index into the line.
 *
 * Returns 1 on success, 0 if a domain error occurred (and threw an exception).
 * -------------------------------------------------------------------------- */
static int reduce_line(const char *p, zend_long stride, zend_long n,
                       numphp_dtype dt_in, numphp_dtype dt_out,
                       numphp_reduce_op op, int ddof, int skip_nan,
                       char *out_ptr)
{
    switch (op) {

    /* ---- SUM ----------------------------------------------------------- */
    case NUMPHP_REDUCE_SUM: {
        if (dt_in == NUMPHP_INT32 || dt_in == NUMPHP_INT64) {
            int64_t s = 0;
            for (zend_long i = 0; i < n; i++) {
                s += numphp_read_i64(p + i * stride, dt_in);
            }
            numphp_write_scalar_at(out_ptr, dt_out, (double)s, (zend_long)s);
        } else if (skip_nan) {
            zend_long c;
            double s = pairwise_sum_f64_skipnan(p, stride, n, dt_in, &c);
            numphp_write_scalar_at(out_ptr, dt_out, s, (zend_long)s);
        } else {
            double s = pairwise_sum_f64(p, stride, n, dt_in);
            numphp_write_scalar_at(out_ptr, dt_out, s, (zend_long)s);
        }
        return 1;
    }

    /* ---- MEAN ---------------------------------------------------------- */
    case NUMPHP_REDUCE_MEAN: {
        if (n == 0) {
            numphp_write_scalar_at(out_ptr, dt_out, NAN, 0);
            return 1;
        }
        double s; zend_long c = n;
        if (skip_nan) {
            s = pairwise_sum_f64_skipnan(p, stride, n, dt_in, &c);
        } else {
            s = pairwise_sum_f64(p, stride, n, dt_in);
        }
        double m = (c > 0) ? (s / (double)c) : NAN;
        numphp_write_scalar_at(out_ptr, dt_out, m, (zend_long)m);
        return 1;
    }

    /* ---- MIN / MAX ----------------------------------------------------- */
    case NUMPHP_REDUCE_MIN:
    case NUMPHP_REDUCE_MAX: {
        int is_int = (dt_in == NUMPHP_INT32 || dt_in == NUMPHP_INT64);
        if (n == 0) {
            /* Caller is supposed to bounds-check; defensive NaN/0 */
            if (is_int) numphp_write_scalar_at(out_ptr, dt_out, 0.0, 0);
            else        numphp_write_scalar_at(out_ptr, dt_out, NAN, 0);
            return 1;
        }
        if (is_int) {
            int64_t best = numphp_read_i64(p, dt_in);
            for (zend_long i = 1; i < n; i++) {
                int64_t v = numphp_read_i64(p + i * stride, dt_in);
                if (op == NUMPHP_REDUCE_MIN ? (v < best) : (v > best)) best = v;
            }
            numphp_write_scalar_at(out_ptr, dt_out, (double)best, (zend_long)best);
        } else {
            /* Float min/max:
             *   default: NaN propagates — if any NaN seen, result is NaN.
             *   skip_nan: ignore NaNs; if all NaN, result is NaN. */
            double best = numphp_read_f64(p, dt_in);
            int saw_nonnan = !isnan(best);
            int saw_nan = isnan(best);
            if (skip_nan && !saw_nonnan) { saw_nan = 1; }  /* track but don't propagate */
            for (zend_long i = 1; i < n; i++) {
                double v = numphp_read_f64(p + i * stride, dt_in);
                if (isnan(v)) {
                    saw_nan = 1;
                    if (!skip_nan) { best = v; break; }
                    continue;
                }
                if (!saw_nonnan) { best = v; saw_nonnan = 1; continue; }
                if (op == NUMPHP_REDUCE_MIN ? (v < best) : (v > best)) best = v;
            }
            if (!skip_nan && saw_nan) best = NAN;
            else if (skip_nan && !saw_nonnan) best = NAN;
            numphp_write_scalar_at(out_ptr, dt_out, best, (zend_long)best);
        }
        return 1;
    }

    /* ---- VAR / STD (Welford) ------------------------------------------ */
    case NUMPHP_REDUCE_VAR:
    case NUMPHP_REDUCE_STD: {
        if (!skip_nan) {
            /* Default: any NaN in input → NaN out. Walk once to detect. */
            for (zend_long i = 0; i < n; i++) {
                double v = numphp_read_f64(p + i * stride, dt_in);
                if (isnan(v)) {
                    numphp_write_scalar_at(out_ptr, dt_out, NAN, 0);
                    return 1;
                }
            }
        }
        double mean, m2; zend_long count;
        welford_axis(p, stride, n, dt_in, skip_nan, &mean, &m2, &count);
        double denom = (double)(count - ddof);
        double var = (count > 0 && denom > 0.0) ? (m2 / denom) : NAN;
        double res = (op == NUMPHP_REDUCE_STD) ? sqrt(var) : var;
        numphp_write_scalar_at(out_ptr, dt_out, res, (zend_long)res);
        return 1;
    }

    /* ---- ARGMIN / ARGMAX ---------------------------------------------- */
    case NUMPHP_REDUCE_ARGMIN:
    case NUMPHP_REDUCE_ARGMAX: {
        if (n == 0) {
            zend_throw_exception_ex(numphp_ndarray_exception_ce, 0,
                "%s: empty array", op == NUMPHP_REDUCE_ARGMIN ? "argmin" : "argmax");
            return 0;
        }
        int is_int = (dt_in == NUMPHP_INT32 || dt_in == NUMPHP_INT64);
        if (is_int) {
            int64_t best = numphp_read_i64(p, dt_in);
            zend_long best_i = 0;
            for (zend_long i = 1; i < n; i++) {
                int64_t v = numphp_read_i64(p + i * stride, dt_in);
                if (op == NUMPHP_REDUCE_ARGMIN ? (v < best) : (v > best)) {
                    best = v; best_i = i;
                }
            }
            numphp_write_scalar_at(out_ptr, dt_out, (double)best_i, best_i);
        } else {
            /* Default behavior on argmin/argmax:
             *   - Without skip_nan: NaN compares as "greater than everything" for
             *     argmin (so it never wins); for argmax it propagates by being the
             *     first NaN's index. Match NumPy: if any NaN, return its first index.
             *   - With skip_nan: skip NaNs entirely; if all NaN, throw. */
            zend_long first_nan = -1;
            for (zend_long i = 0; i < n; i++) {
                double v = numphp_read_f64(p + i * stride, dt_in);
                if (isnan(v)) { first_nan = i; break; }
            }
            if (!skip_nan && first_nan >= 0) {
                numphp_write_scalar_at(out_ptr, dt_out, (double)first_nan, first_nan);
                return 1;
            }
            zend_long start = -1;
            double best = 0.0;
            for (zend_long i = 0; i < n; i++) {
                double v = numphp_read_f64(p + i * stride, dt_in);
                if (isnan(v)) continue;
                start = i; best = v; break;
            }
            if (start < 0) {
                /* All NaN with skip_nan */
                zend_throw_exception_ex(numphp_ndarray_exception_ce, 0,
                    "%s: all-NaN slice", op == NUMPHP_REDUCE_ARGMIN ? "nanargmin" : "nanargmax");
                return 0;
            }
            zend_long best_i = start;
            for (zend_long i = start + 1; i < n; i++) {
                double v = numphp_read_f64(p + i * stride, dt_in);
                if (isnan(v)) continue;
                if (op == NUMPHP_REDUCE_ARGMIN ? (v < best) : (v > best)) {
                    best = v; best_i = i;
                }
            }
            numphp_write_scalar_at(out_ptr, dt_out, (double)best_i, best_i);
        }
        return 1;
    }
    } /* switch */
    return 0;
}

/* ----------------------------------------------------------------------------
 * Public reduction API.
 *
 * Strategy:
 *   - Materialise `src` to a fresh contiguous owner if it isn't already.
 *     This lets the global-reduction path treat the data as a flat 1-D line,
 *     and the axis-reduction path uses well-defined strides.
 *   - Build the output shape (drop axis, or keep size 1, or 0-D).
 *   - For axis reductions, walk the outer indices; for each, compute the line
 *     reduction and store at the matching output position.
 * -------------------------------------------------------------------------- */
numphp_ndarray *numphp_reduce(numphp_ndarray *src, numphp_reduce_op op,
                              int has_axis, int axis, int keepdims, int ddof, int skip_nan)
{
    numphp_dtype out_dt = reduce_out_dtype(op, src->dtype);

    /* Normalise / bounds-check axis at the C layer too, so direct callers are safe. */
    if (has_axis) {
        int ax = axis;
        if (ax < 0) ax += src->ndim;
        if (src->ndim == 0 || ax < 0 || ax >= src->ndim) {
            zend_throw_exception_ex(numphp_shape_exception_ce, 0,
                "axis %d out of range for ndim %d", axis, src->ndim);
            return NULL;
        }
        axis = ax;
    }

    /* Empty global reduction (size 0) */
    if (!has_axis && src->size == 0) {
        if (op == NUMPHP_REDUCE_ARGMIN || op == NUMPHP_REDUCE_ARGMAX) {
            zend_throw_exception_ex(numphp_ndarray_exception_ce, 0,
                "%s: empty array",
                op == NUMPHP_REDUCE_ARGMIN ? "argmin" : "argmax");
            return NULL;
        }
        /* Build 0-D or all-1 keepdims output */
        zend_long shape[NUMPHP_MAX_NDIM] = {0};
        int ndim_out;
        if (keepdims) {
            ndim_out = src->ndim;
            for (int i = 0; i < ndim_out; i++) shape[i] = 1;
        } else {
            ndim_out = 0;
        }
        numphp_ndarray *out = numphp_ndarray_alloc_owner(out_dt, ndim_out, shape);
        char *o = (char *)out->buffer->data;
        if (op == NUMPHP_REDUCE_SUM)       numphp_write_scalar_at(o, out_dt, 0.0, 0);
        else if (op == NUMPHP_REDUCE_MEAN) numphp_write_scalar_at(o, out_dt, NAN, 0);
        else                               numphp_write_scalar_at(o, out_dt, NAN, 0);
        return out;
    }

    /* Use a contiguous copy for the inner walk. */
    numphp_ndarray *contig = (src->flags & NUMPHP_C_CONTIGUOUS)
        ? src : numphp_materialize_contiguous(src);
    int owns_contig = (contig != src);
    char *src_base = (char *)contig->buffer->data + contig->offset;

    /* ===== global reduction ===== */
    if (!has_axis) {
        zend_long shape[NUMPHP_MAX_NDIM] = {0};
        int ndim_out;
        if (keepdims) {
            ndim_out = src->ndim;
            for (int i = 0; i < ndim_out; i++) shape[i] = 1;
        } else {
            ndim_out = 0;
        }
        numphp_ndarray *out = numphp_ndarray_alloc_owner(out_dt, ndim_out, shape);
        char *o = (char *)out->buffer->data;

        zend_long n = contig->size;
        zend_long stride = contig->itemsize;
        if (!reduce_line(src_base, stride, n, src->dtype, out_dt, op, ddof, skip_nan, o)) {
            numphp_ndarray_free(out);
            if (owns_contig) numphp_ndarray_free(contig);
            return NULL;
        }
        if (owns_contig) numphp_ndarray_free(contig);
        return out;
    }

    /* ===== axis reduction ===== */
    int red = axis;
    int in_ndim = src->ndim;
    zend_long axis_n = src->shape[red];
    zend_long axis_stride = contig->strides[red];

    /* Output shape: drop or keep size-1 along `red`. */
    zend_long out_shape[NUMPHP_MAX_NDIM];
    int out_ndim;
    if (keepdims) {
        out_ndim = in_ndim;
        for (int i = 0; i < in_ndim; i++) out_shape[i] = (i == red) ? 1 : src->shape[i];
    } else {
        out_ndim = in_ndim - 1;
        int j = 0;
        for (int i = 0; i < in_ndim; i++) {
            if (i == red) continue;
            out_shape[j++] = src->shape[i];
        }
    }
    numphp_ndarray *out = numphp_ndarray_alloc_owner(out_dt, out_ndim, out_shape);
    char *out_base = (char *)out->buffer->data;
    zend_long out_itemsize = out->itemsize;

    /* For each combination of "outer" indices (every axis except `red`):
     *   - compute the source pointer
     *   - compute the destination pointer
     *   - reduce_line() over the red axis */
    zend_long idx[NUMPHP_MAX_NDIM] = {0};
    /* The number of outer iterations = src->size / axis_n, except when axis_n == 0 */
    zend_long outer_total = (axis_n > 0) ? (src->size / axis_n) : 0;

    if (axis_n == 0) {
        /* Reducing along a zero-length axis. argmin/argmax → throw. Others fill identity / NaN. */
        if (op == NUMPHP_REDUCE_ARGMIN || op == NUMPHP_REDUCE_ARGMAX) {
            zend_throw_exception_ex(numphp_ndarray_exception_ce, 0,
                "%s: zero-length reduction axis",
                op == NUMPHP_REDUCE_ARGMIN ? "argmin" : "argmax");
            numphp_ndarray_free(out);
            if (owns_contig) numphp_ndarray_free(contig);
            return NULL;
        }
        zend_long out_size = out->size;
        for (zend_long n = 0; n < out_size; n++) {
            char *o = out_base + n * out_itemsize;
            if (op == NUMPHP_REDUCE_SUM) numphp_write_scalar_at(o, out_dt, 0.0, 0);
            else                          numphp_write_scalar_at(o, out_dt, NAN, 0);
        }
        if (owns_contig) numphp_ndarray_free(contig);
        return out;
    }

    /* Fast path: 2-D C-contiguous f32/f64 source, axis-0 SUM, no NaN-skip.
     * Tiled column kernel — see axis0_sum_*_2d above. Bit-identical to the
     * slow path because per-column pairwise recursion is preserved. */
    if (op == NUMPHP_REDUCE_SUM && !skip_nan
        && in_ndim == 2 && red == 0
        && (src->dtype == NUMPHP_FLOAT64 || src->dtype == NUMPHP_FLOAT32)
        && out_dt == src->dtype) {
        zend_long nrows = src->shape[0];
        zend_long ncols = src->shape[1];
        if (src->dtype == NUMPHP_FLOAT64) {
            axis0_sum_f64_2d((const double *)src_base, nrows, ncols,
                             (double *)out_base);
        } else {
            axis0_sum_f32_2d((const float *)src_base, nrows, ncols,
                             (float *)out_base);
        }
        if (owns_contig) numphp_ndarray_free(contig);
        return out;
    }

    for (zend_long n = 0; n < outer_total; n++) {
        /* Compute src offset by combining all axes except red. */
        zend_long src_off = 0;
        for (int i = 0; i < in_ndim; i++) {
            if (i == red) continue;
            src_off += idx[i] * contig->strides[i];
        }
        char *o = out_base + n * out_itemsize;
        if (!reduce_line(src_base + src_off, axis_stride, axis_n,
                         src->dtype, out_dt, op, ddof, skip_nan, o)) {
            numphp_ndarray_free(out);
            if (owns_contig) numphp_ndarray_free(contig);
            return NULL;
        }
        /* Increment all outer axes (skipping red), inner-most first. */
        for (int i = in_ndim - 1; i >= 0; i--) {
            if (i == red) continue;
            if (++idx[i] < src->shape[i]) break;
            idx[i] = 0;
        }
    }

    if (owns_contig) numphp_ndarray_free(contig);
    return out;
}

/* ============================================================================
 * Cumulative reductions — cumsum / cumprod / nancumsum / nancumprod
 * ----------------------------------------------------------------------------
 * Output dtype rules (locked by docs/system.md decision 31):
 *   int32 / int64 → int64;  float32 → float32;  float64 → float64.
 * `cumprod` on int promotes to int64 — divergence from NumPy, motivated by the
 * same silent-overflow concern that drove decision 9 for `sum`.
 *
 * Layout: the public entry point promotes the input to a contiguous buffer of
 * the output dtype (via numphp_ensure_contig_dtype), allocates a same-shape
 * output owner, then dispatches to a typed inner-loop helper for each line
 * along the chosen axis. For `axis = null` (has_axis = 0), input is flattened
 * and the output is 1-D of size src->size — a single line.
 * ========================================================================== */

#define CUMSUM_IDENTITY  0.0
#define CUMPROD_IDENTITY 1.0

static inline double cum_combine_f64(double acc, double v, numphp_cumulative_op op)
{
    return (op == NUMPHP_CUM_SUM) ? (acc + v) : (acc * v);
}

static void cum_line_f64(const double *src, double *dst,
                         zend_long n, zend_long src_step, zend_long dst_step,
                         numphp_cumulative_op op, int skip_nan)
{
    double acc = (op == NUMPHP_CUM_SUM) ? CUMSUM_IDENTITY : CUMPROD_IDENTITY;
    for (zend_long i = 0; i < n; i++) {
        double v = src[i * src_step];
        if (skip_nan && isnan(v)) {
            /* treat as identity — leave acc unchanged */
        } else {
            acc = cum_combine_f64(acc, v, op);
        }
        dst[i * dst_step] = acc;
    }
}

static inline float cum_combine_f32(float acc, float v, numphp_cumulative_op op)
{
    return (op == NUMPHP_CUM_SUM) ? (acc + v) : (acc * v);
}

static void cum_line_f32(const float *src, float *dst,
                         zend_long n, zend_long src_step, zend_long dst_step,
                         numphp_cumulative_op op, int skip_nan)
{
    float acc = (op == NUMPHP_CUM_SUM) ? 0.0f : 1.0f;
    for (zend_long i = 0; i < n; i++) {
        float v = src[i * src_step];
        if (skip_nan && isnan(v)) {
            /* identity */
        } else {
            acc = cum_combine_f32(acc, v, op);
        }
        dst[i * dst_step] = acc;
    }
}

static void cum_line_i64(const int64_t *src, int64_t *dst,
                         zend_long n, zend_long src_step, zend_long dst_step,
                         numphp_cumulative_op op)
{
    int64_t acc = (op == NUMPHP_CUM_SUM) ? 0 : 1;
    for (zend_long i = 0; i < n; i++) {
        int64_t v = src[i * src_step];
        acc = (op == NUMPHP_CUM_SUM) ? (acc + v) : (acc * v);
        dst[i * dst_step] = acc;
    }
}

static numphp_dtype cumulative_out_dtype(numphp_dtype in)
{
    if (in == NUMPHP_INT32 || in == NUMPHP_INT64 || in == NUMPHP_BOOL) return NUMPHP_INT64;
    return in;
}

numphp_ndarray *numphp_cumulative(numphp_ndarray *src, numphp_cumulative_op op,
                                  int has_axis, int axis, int skip_nan)
{
    numphp_dtype out_dt = cumulative_out_dtype(src->dtype);

    /* Integer dtypes have no NaN — NaN-skip is a no-op. Match the alias
     * convention used by the regular reductions. */
    int eff_skip = skip_nan;
    if (out_dt == NUMPHP_INT64) eff_skip = 0;

    /* Normalise / bounds-check axis. Allowed range matches reductions. */
    int red = 0;
    if (has_axis) {
        red = axis;
        if (red < 0) red += src->ndim;
        if (src->ndim == 0 || red < 0 || red >= src->ndim) {
            zend_throw_exception_ex(numphp_shape_exception_ce, 0,
                "axis %d out of range for ndim %d", axis, src->ndim);
            return NULL;
        }
    }

    /* For axis=null (flatten): output is 1-D of length src->size. We get a
     * contiguous, dtype-promoted view via ensure_contig_dtype and run a single
     * line. */
    if (!has_axis) {
        int owned = 0;
        numphp_ndarray *contig = numphp_ensure_contig_dtype(src, out_dt, &owned);
        if (!contig) return NULL;

        zend_long shape[1] = { src->size };
        numphp_ndarray *out = numphp_ndarray_alloc_owner(out_dt, 1, shape);
        if (src->size > 0) {
            char *sp = (char *)contig->buffer->data + contig->offset;
            char *op_ptr = (char *)out->buffer->data;
            switch (out_dt) {
                case NUMPHP_FLOAT64:
                    cum_line_f64((const double *)sp, (double *)op_ptr,
                                 src->size, 1, 1, op, eff_skip);
                    break;
                case NUMPHP_FLOAT32:
                    cum_line_f32((const float *)sp, (float *)op_ptr,
                                 src->size, 1, 1, op, eff_skip);
                    break;
                case NUMPHP_INT64:
                    cum_line_i64((const int64_t *)sp, (int64_t *)op_ptr,
                                 src->size, 1, 1, op);
                    break;
                default:
                    /* unreachable — out_dt restricted by cumulative_out_dtype */
                    break;
            }
        }
        if (owned) numphp_ndarray_free(contig);
        return out;
    }

    /* ===== axis cumulation ===== */
    int in_ndim = src->ndim;
    zend_long axis_n = src->shape[red];

    /* Promote / contiguify so we can index by element-stride in items. */
    int owned = 0;
    numphp_ndarray *contig = numphp_ensure_contig_dtype(src, out_dt, &owned);
    if (!contig) return NULL;

    /* Allocate output of input's shape (no shape change for axis cumulation). */
    numphp_ndarray *out = numphp_ndarray_alloc_owner(out_dt, in_ndim, src->shape);

    /* Empty input → output also has size 0 (same shape); no data to write. */
    if (src->size == 0 || axis_n == 0) {
        if (owned) numphp_ndarray_free(contig);
        return out;
    }

    char *src_base = (char *)contig->buffer->data + contig->offset;
    char *out_base = (char *)out->buffer->data;
    zend_long itemsize = out->itemsize;

    /* Both contig and out are C-contiguous of out_dt; strides[red] / itemsize
     * gives the element-step along the cumulating axis. */
    zend_long src_step_items = contig->strides[red] / itemsize;
    zend_long out_step_items = out->strides[red] / itemsize;

    zend_long outer_total = src->size / axis_n;
    zend_long idx[NUMPHP_MAX_NDIM] = {0};

    for (zend_long n = 0; n < outer_total; n++) {
        zend_long src_off = 0;
        zend_long dst_off = 0;
        for (int i = 0; i < in_ndim; i++) {
            if (i == red) continue;
            src_off += idx[i] * contig->strides[i];
            dst_off += idx[i] * out->strides[i];
        }
        const char *sp = src_base + src_off;
        char *op_ptr = out_base + dst_off;

        switch (out_dt) {
            case NUMPHP_FLOAT64:
                cum_line_f64((const double *)sp, (double *)op_ptr,
                             axis_n, src_step_items, out_step_items, op, eff_skip);
                break;
            case NUMPHP_FLOAT32:
                cum_line_f32((const float *)sp, (float *)op_ptr,
                             axis_n, src_step_items, out_step_items, op, eff_skip);
                break;
            case NUMPHP_INT64:
                cum_line_i64((const int64_t *)sp, (int64_t *)op_ptr,
                             axis_n, src_step_items, out_step_items, op);
                break;
            default:
                break;
        }

        /* Increment outer indices, skipping the cumulating axis. */
        for (int i = in_ndim - 1; i >= 0; i--) {
            if (i == red) continue;
            if (++idx[i] < src->shape[i]) break;
            idx[i] = 0;
        }
    }

    if (owned) numphp_ndarray_free(contig);
    return out;
}

/* ============================================================================
 * Comparison ops — eq / ne / lt / le / gt / ge → bool
 * ----------------------------------------------------------------------------
 * Inputs are promoted to a common dtype before comparing. Output is always
 * NUMPHP_BOOL of the broadcast shape. NaN policy (decision 33):
 *   eq / lt / le / gt / ge → false if either operand is NaN
 *   ne                     → true  if either operand is NaN  (IEEE 754)
 * Integer-input fast path skips the NaN test (no NaNs in int).
 * ========================================================================== */

static inline uint8_t cmp_compute_double(double a, double b, numphp_compare_op op)
{
    if (isnan(a) || isnan(b)) {
        return (op == NUMPHP_CMP_NE) ? 1 : 0;
    }
    switch (op) {
        case NUMPHP_CMP_EQ: return a == b ? 1 : 0;
        case NUMPHP_CMP_NE: return a != b ? 1 : 0;
        case NUMPHP_CMP_LT: return a <  b ? 1 : 0;
        case NUMPHP_CMP_LE: return a <= b ? 1 : 0;
        case NUMPHP_CMP_GT: return a >  b ? 1 : 0;
        case NUMPHP_CMP_GE: return a >= b ? 1 : 0;
    }
    return 0;
}

static inline uint8_t cmp_compute_int64(int64_t a, int64_t b, numphp_compare_op op)
{
    switch (op) {
        case NUMPHP_CMP_EQ: return a == b ? 1 : 0;
        case NUMPHP_CMP_NE: return a != b ? 1 : 0;
        case NUMPHP_CMP_LT: return a <  b ? 1 : 0;
        case NUMPHP_CMP_LE: return a <= b ? 1 : 0;
        case NUMPHP_CMP_GT: return a >  b ? 1 : 0;
        case NUMPHP_CMP_GE: return a >= b ? 1 : 0;
    }
    return 0;
}

numphp_ndarray *numphp_compare(numphp_ndarray *a, numphp_ndarray *b,
                               numphp_compare_op op)
{
    int out_ndim;
    zend_long out_shape[NUMPHP_MAX_NDIM];
    numphp_ndarray *ops[2] = { a, b };
    if (!numphp_broadcast_shape(2, ops, &out_ndim, out_shape)) return NULL;

    numphp_dtype cmp_dt = numphp_promote_dtype(a->dtype, b->dtype);
    int int_path = (cmp_dt == NUMPHP_INT32 || cmp_dt == NUMPHP_INT64
                 || cmp_dt == NUMPHP_BOOL);

    numphp_ndarray *out = numphp_ndarray_alloc_owner(NUMPHP_BOOL, out_ndim, out_shape);

    numphp_nditer it;
    if (!numphp_nditer_init(&it, 2, ops, out)) {
        numphp_ndarray_free(out);
        return NULL;
    }

    if (it.size > 0) do {
        uint8_t r;
        if (int_path) {
            int64_t av = numphp_read_i64(it.ptr[0], a->dtype);
            int64_t bv = numphp_read_i64(it.ptr[1], b->dtype);
            r = cmp_compute_int64(av, bv, op);
        } else {
            double av = numphp_read_f64(it.ptr[0], a->dtype);
            double bv = numphp_read_f64(it.ptr[1], b->dtype);
            r = cmp_compute_double(av, bv, op);
        }
        *(uint8_t *)it.ptr[2] = r;
        numphp_nditer_next(&it);
    } while (!it.done);

    return out;
}

/* ============================================================================
 * where(cond, x, y) — element-wise select
 * ----------------------------------------------------------------------------
 * cond must be NUMPHP_BOOL; throws \DTypeException otherwise. Output dtype is
 * the promotion of x and y (cond's dtype is intentionally irrelevant — story
 * 17 rule). Broadcasts across all three operands.
 * ========================================================================== */

numphp_ndarray *numphp_where(numphp_ndarray *cond,
                             numphp_ndarray *x,
                             numphp_ndarray *y)
{
    if (cond->dtype != NUMPHP_BOOL) {
        zend_throw_exception_ex(numphp_dtype_exception_ce, 0,
            "where: cond must be bool dtype, got %s",
            numphp_dtype_name(cond->dtype));
        return NULL;
    }

    int out_ndim;
    zend_long out_shape[NUMPHP_MAX_NDIM];
    numphp_ndarray *ops[3] = { cond, x, y };
    if (!numphp_broadcast_shape(3, ops, &out_ndim, out_shape)) return NULL;

    numphp_dtype out_dt = numphp_promote_dtype(x->dtype, y->dtype);
    numphp_ndarray *out = numphp_ndarray_alloc_owner(out_dt, out_ndim, out_shape);

    numphp_nditer it;
    if (!numphp_nditer_init(&it, 3, ops, out)) {
        numphp_ndarray_free(out);
        return NULL;
    }

    if (it.size > 0) do {
        uint8_t c = *(const uint8_t *)it.ptr[0];
        const char *src = c ? it.ptr[1] : it.ptr[2];
        numphp_dtype src_dt = c ? x->dtype : y->dtype;
        char *dst = it.ptr[3];

        switch (out_dt) {
            case NUMPHP_FLOAT64:
                *(double *)dst = numphp_read_f64(src, src_dt);
                break;
            case NUMPHP_FLOAT32:
                *(float *)dst = numphp_read_f32(src, src_dt);
                break;
            case NUMPHP_INT64:
                *(int64_t *)dst = numphp_read_i64(src, src_dt);
                break;
            case NUMPHP_INT32:
                *(int32_t *)dst = (int32_t)numphp_read_i64(src, src_dt);
                break;
            case NUMPHP_BOOL: {
                int64_t v = numphp_read_i64(src, src_dt);
                *(uint8_t *)dst = (v != 0) ? 1 : 0;
                break;
            }
        }
        numphp_nditer_next(&it);
    } while (!it.done);

    return out;
}

/* ============================================================================
 * Element-wise math
 * ============================================================================ */

static numphp_dtype unary_out_dtype(numphp_math_op op, numphp_dtype in)
{
    switch (op) {
        case NUMPHP_MATH_SQRT:
            if (in == NUMPHP_INT32 || in == NUMPHP_INT64) return NUMPHP_FLOAT64;
            return in;
        case NUMPHP_MATH_EXP:
        case NUMPHP_MATH_LOG:
        case NUMPHP_MATH_LOG2:
        case NUMPHP_MATH_LOG10:
            return NUMPHP_FLOAT64;
        case NUMPHP_MATH_ABS:
        case NUMPHP_MATH_FLOOR:
        case NUMPHP_MATH_CEIL:
            return in;
    }
    return in;
}

static double apply_double(numphp_math_op op, double x)
{
    switch (op) {
        case NUMPHP_MATH_SQRT:  return sqrt(x);
        case NUMPHP_MATH_EXP:   return exp(x);
        case NUMPHP_MATH_LOG:   return log(x);
        case NUMPHP_MATH_LOG2:  return log2(x);
        case NUMPHP_MATH_LOG10: return log10(x);
        case NUMPHP_MATH_ABS:   return fabs(x);
        case NUMPHP_MATH_FLOOR: return floor(x);
        case NUMPHP_MATH_CEIL:  return ceil(x);
    }
    return x;
}

numphp_ndarray *numphp_apply_unary(numphp_ndarray *src, numphp_math_op op)
{
    numphp_dtype out_dt = unary_out_dtype(op, src->dtype);
    numphp_ndarray *out = numphp_ndarray_alloc_owner(out_dt, src->ndim, src->shape);
    if (out->size == 0) return out;

    char *src_base = (char *)src->buffer->data + src->offset;
    char *dst = (char *)out->buffer->data;
    zend_long out_size = out->itemsize;

    /* Fast path: f32 sqrt stays in f32 (use sqrtf); abs/floor/ceil on int are no-ops. */
    int int_in = (src->dtype == NUMPHP_INT32 || src->dtype == NUMPHP_INT64);

    if (int_in && (op == NUMPHP_MATH_FLOOR || op == NUMPHP_MATH_CEIL)) {
        /* No-op: copy through preserving dtype. */
        zend_long idx[NUMPHP_MAX_NDIM] = {0};
        for (zend_long n = 0; n < src->size; n++) {
            zend_long s_off = 0;
            for (int j = 0; j < src->ndim; j++) s_off += idx[j] * src->strides[j];
            double dv = 0.0; zend_long lv = 0;
            numphp_read_scalar_at(src_base + s_off, src->dtype, &dv, &lv);
            numphp_write_scalar_at(dst + n * out_size, out_dt, dv, lv);
            for (int j = src->ndim - 1; j >= 0; j--) {
                if (++idx[j] < src->shape[j]) break;
                idx[j] = 0;
            }
        }
        return out;
    }

    if (int_in && op == NUMPHP_MATH_ABS) {
        zend_long idx[NUMPHP_MAX_NDIM] = {0};
        for (zend_long n = 0; n < src->size; n++) {
            zend_long s_off = 0;
            for (int j = 0; j < src->ndim; j++) s_off += idx[j] * src->strides[j];
            int64_t v = numphp_read_i64(src_base + s_off, src->dtype);
            int64_t r = (v < 0) ? -v : v;
            numphp_write_scalar_at(dst + n * out_size, out_dt, (double)r, (zend_long)r);
            for (int j = src->ndim - 1; j >= 0; j--) {
                if (++idx[j] < src->shape[j]) break;
                idx[j] = 0;
            }
        }
        return out;
    }

    /* General path: walk src in declared order, compute in double, store typed. */
    zend_long idx[NUMPHP_MAX_NDIM] = {0};
    for (zend_long n = 0; n < src->size; n++) {
        zend_long s_off = 0;
        for (int j = 0; j < src->ndim; j++) s_off += idx[j] * src->strides[j];
        double x = numphp_read_f64(src_base + s_off, src->dtype);
        double r = apply_double(op, x);
        numphp_write_scalar_at(dst + n * out_size, out_dt, r, (zend_long)r);
        for (int j = src->ndim - 1; j >= 0; j--) {
            if (++idx[j] < src->shape[j]) break;
            idx[j] = 0;
        }
    }
    return out;
}

numphp_ndarray *numphp_apply_clip(numphp_ndarray *src, int has_min, double minv, int has_max, double maxv)
{
    numphp_ndarray *out = numphp_ndarray_alloc_owner(src->dtype, src->ndim, src->shape);
    if (out->size == 0) return out;

    char *src_base = (char *)src->buffer->data + src->offset;
    char *dst = (char *)out->buffer->data;
    zend_long itemsize = out->itemsize;
    int is_int = (src->dtype == NUMPHP_INT32 || src->dtype == NUMPHP_INT64);

    zend_long idx[NUMPHP_MAX_NDIM] = {0};
    for (zend_long n = 0; n < src->size; n++) {
        zend_long s_off = 0;
        for (int j = 0; j < src->ndim; j++) s_off += idx[j] * src->strides[j];

        if (is_int) {
            int64_t v = numphp_read_i64(src_base + s_off, src->dtype);
            if (has_min && (double)v < minv) v = (int64_t)minv;
            if (has_max && (double)v > maxv) v = (int64_t)maxv;
            numphp_write_scalar_at(dst + n * itemsize, src->dtype, (double)v, (zend_long)v);
        } else {
            double v = numphp_read_f64(src_base + s_off, src->dtype);
            if (has_min && v < minv) v = minv;
            if (has_max && v > maxv) v = maxv;
            numphp_write_scalar_at(dst + n * itemsize, src->dtype, v, (zend_long)v);
        }

        for (int j = src->ndim - 1; j >= 0; j--) {
            if (++idx[j] < src->shape[j]) break;
            idx[j] = 0;
        }
    }
    return out;
}

/* PHP-compatible round-half-away-from-zero. Matches PHP's default round() mode
 * (PHP_ROUND_HALF_UP). DELIBERATELY differs from NumPy's banker's rounding.
 * See docs/sprints/09-stats-and-math-functions.md "round-half semantics". */
static double round_half_away_from_zero(double x, int decimals)
{
    if (isnan(x) || isinf(x)) return x;
    double f = pow(10.0, (double)decimals);
    if (x >= 0) return floor(x * f + 0.5) / f;
    return -floor(-x * f + 0.5) / f;
}

numphp_ndarray *numphp_apply_round(numphp_ndarray *src, int decimals)
{
    numphp_ndarray *out = numphp_ndarray_alloc_owner(src->dtype, src->ndim, src->shape);
    if (out->size == 0) return out;

    char *src_base = (char *)src->buffer->data + src->offset;
    char *dst = (char *)out->buffer->data;
    zend_long itemsize = out->itemsize;
    int is_int = (src->dtype == NUMPHP_INT32 || src->dtype == NUMPHP_INT64);

    zend_long idx[NUMPHP_MAX_NDIM] = {0};
    for (zend_long n = 0; n < src->size; n++) {
        zend_long s_off = 0;
        for (int j = 0; j < src->ndim; j++) s_off += idx[j] * src->strides[j];

        if (is_int) {
            /* Integer round → identity (decimals shouldn't matter for int values). */
            double dv = 0.0; zend_long lv = 0;
            numphp_read_scalar_at(src_base + s_off, src->dtype, &dv, &lv);
            numphp_write_scalar_at(dst + n * itemsize, src->dtype, dv, lv);
        } else {
            double v = numphp_read_f64(src_base + s_off, src->dtype);
            v = round_half_away_from_zero(v, decimals);
            numphp_write_scalar_at(dst + n * itemsize, src->dtype, v, (zend_long)v);
        }

        for (int j = src->ndim - 1; j >= 0; j--) {
            if (++idx[j] < src->shape[j]) break;
            idx[j] = 0;
        }
    }
    return out;
}

numphp_ndarray *numphp_apply_power_scalar(numphp_ndarray *src, double exponent)
{
    /* Output dtype: integer base + integer exponent → preserve input dtype.
     * Otherwise → float64. (NumPy's pow promotes int**int to int.) */
    int int_in = (src->dtype == NUMPHP_INT32 || src->dtype == NUMPHP_INT64);
    int int_exp = (exponent == floor(exponent) && exponent >= 0.0);
    numphp_dtype out_dt = (int_in && int_exp) ? src->dtype : NUMPHP_FLOAT64;

    numphp_ndarray *out = numphp_ndarray_alloc_owner(out_dt, src->ndim, src->shape);
    if (out->size == 0) return out;

    char *src_base = (char *)src->buffer->data + src->offset;
    char *dst = (char *)out->buffer->data;
    zend_long itemsize = out->itemsize;

    zend_long idx[NUMPHP_MAX_NDIM] = {0};
    for (zend_long n = 0; n < src->size; n++) {
        zend_long s_off = 0;
        for (int j = 0; j < src->ndim; j++) s_off += idx[j] * src->strides[j];
        double x = numphp_read_f64(src_base + s_off, src->dtype);
        double r = pow(x, exponent);
        numphp_write_scalar_at(dst + n * itemsize, out_dt, r, (zend_long)r);
        for (int j = src->ndim - 1; j >= 0; j--) {
            if (++idx[j] < src->shape[j]) break;
            idx[j] = 0;
        }
    }
    return out;
}

/* ============================================================================
 * sort / argsort
 *
 * Strategy:
 *   - axis = NUMPHP_AXIS_FLATTEN → flatten to 1-D contiguous, sort, return 1-D.
 *   - else: walk outer indices, gather the strided line into a contiguous
 *     scratch buffer, qsort, scatter back into the output.
 *
 * qsort is unstable for equal keys; documented in the sprint plan.
 * ============================================================================ */

/* Comparators in double-precision land. Argsort piggy-backs by sorting an
 * (index, value) pair table. We use thread_local-ish module statics — fine for
 * NTS-only. */
static double *g_sort_values = NULL;

static int cmp_idx_by_value_asc(const void *pa, const void *pb)
{
    zend_long ia = *(const zend_long *)pa;
    zend_long ib = *(const zend_long *)pb;
    double va = g_sort_values[ia];
    double vb = g_sort_values[ib];
    /* NaNs sort to the end (NumPy convention). */
    int na = isnan(va), nb = isnan(vb);
    if (na && nb) return 0;
    if (na) return 1;
    if (nb) return -1;
    if (va < vb) return -1;
    if (va > vb) return 1;
    return 0;
}

static int cmp_double_asc(const void *pa, const void *pb)
{
    double a = *(const double *)pa;
    double b = *(const double *)pb;
    int na = isnan(a), nb = isnan(b);
    if (na && nb) return 0;
    if (na) return 1;
    if (nb) return -1;
    if (a < b) return -1;
    if (a > b) return 1;
    return 0;
}

static int cmp_int64_asc(const void *pa, const void *pb)
{
    int64_t a = *(const int64_t *)pa;
    int64_t b = *(const int64_t *)pb;
    if (a < b) return -1;
    if (a > b) return 1;
    return 0;
}

/* Sort one strided line into out_ptr (also strided). Compute in input dtype to
 * avoid roundtripping; specialise int64 / int32 / f32 / f64. */
static void sort_line(const char *p, zend_long stride, zend_long n,
                      numphp_dtype dt,
                      char *out_ptr, zend_long out_stride)
{
    if (n <= 0) return;

    if (dt == NUMPHP_INT64 || dt == NUMPHP_INT32) {
        int64_t *buf = emalloc(sizeof(int64_t) * n);
        for (zend_long i = 0; i < n; i++) buf[i] = numphp_read_i64(p + i * stride, dt);
        qsort(buf, (size_t)n, sizeof(int64_t), cmp_int64_asc);
        for (zend_long i = 0; i < n; i++) {
            numphp_write_scalar_at(out_ptr + i * out_stride, dt, (double)buf[i], (zend_long)buf[i]);
        }
        efree(buf);
        return;
    }

    /* Float path (f32 or f64). Use double scratch. */
    double *buf = emalloc(sizeof(double) * n);
    for (zend_long i = 0; i < n; i++) buf[i] = numphp_read_f64(p + i * stride, dt);
    qsort(buf, (size_t)n, sizeof(double), cmp_double_asc);
    for (zend_long i = 0; i < n; i++) {
        numphp_write_scalar_at(out_ptr + i * out_stride, dt, buf[i], (zend_long)buf[i]);
    }
    efree(buf);
}

/* Argsort one strided line. Output is int64. */
static void argsort_line(const char *p, zend_long stride, zend_long n,
                         numphp_dtype dt,
                         char *out_ptr, zend_long out_stride)
{
    if (n <= 0) return;

    /* Materialise values into a flat double array, then sort an index array against it. */
    double *vals = emalloc(sizeof(double) * n);
    for (zend_long i = 0; i < n; i++) vals[i] = numphp_read_f64(p + i * stride, dt);

    zend_long *idx = emalloc(sizeof(zend_long) * n);
    for (zend_long i = 0; i < n; i++) idx[i] = i;

    g_sort_values = vals;
    qsort(idx, (size_t)n, sizeof(zend_long), cmp_idx_by_value_asc);
    g_sort_values = NULL;

    for (zend_long i = 0; i < n; i++) {
        *(int64_t *)(out_ptr + i * out_stride) = (int64_t)idx[i];
    }

    efree(idx);
    efree(vals);
}

static numphp_ndarray *sort_or_argsort(numphp_ndarray *src, int axis, int do_argsort)
{
    /* Flatten case */
    if (axis == NUMPHP_AXIS_FLATTEN || src->ndim == 0) {
        numphp_ndarray *contig = numphp_materialize_contiguous(src);
        zend_long flat_shape[1] = { contig->size };
        numphp_dtype out_dt = do_argsort ? NUMPHP_INT64 : src->dtype;
        numphp_ndarray *out = numphp_ndarray_alloc_owner(out_dt, 1, flat_shape);
        if (contig->size > 0) {
            char *src_p = (char *)contig->buffer->data + contig->offset;
            char *dst   = (char *)out->buffer->data;
            zend_long s_stride = contig->itemsize;
            zend_long d_stride = out->itemsize;
            if (do_argsort) argsort_line(src_p, s_stride, contig->size, src->dtype, dst, d_stride);
            else            sort_line   (src_p, s_stride, contig->size, src->dtype, dst, d_stride);
        }
        if (contig != src) numphp_ndarray_free(contig);
        return out;
    }

    /* Axis case */
    int ax = axis;
    if (ax < 0) ax += src->ndim;
    if (ax < 0 || ax >= src->ndim) {
        zend_throw_exception_ex(numphp_shape_exception_ce, 0,
            "axis %d out of range for ndim %d", axis, src->ndim);
        return NULL;
    }

    numphp_ndarray *contig = (src->flags & NUMPHP_C_CONTIGUOUS)
        ? src : numphp_materialize_contiguous(src);
    int owns_contig = (contig != src);

    numphp_dtype out_dt = do_argsort ? NUMPHP_INT64 : src->dtype;
    numphp_ndarray *out = numphp_ndarray_alloc_owner(out_dt, src->ndim, src->shape);

    char *src_base = (char *)contig->buffer->data + contig->offset;
    char *dst_base = (char *)out->buffer->data;
    zend_long axis_n = src->shape[ax];
    zend_long src_axis_stride = contig->strides[ax];
    zend_long dst_axis_stride = out->strides[ax];

    /* Walk all outer combinations. */
    zend_long idx[NUMPHP_MAX_NDIM] = {0};
    zend_long outer_total = (axis_n > 0) ? (src->size / axis_n) : 0;

    for (zend_long n = 0; n < outer_total; n++) {
        zend_long s_off = 0, d_off = 0;
        for (int i = 0; i < src->ndim; i++) {
            if (i == ax) continue;
            s_off += idx[i] * contig->strides[i];
            d_off += idx[i] * out->strides[i];
        }
        if (do_argsort) {
            argsort_line(src_base + s_off, src_axis_stride, axis_n, src->dtype,
                         dst_base + d_off, dst_axis_stride);
        } else {
            sort_line(src_base + s_off, src_axis_stride, axis_n, src->dtype,
                      dst_base + d_off, dst_axis_stride);
        }
        for (int i = src->ndim - 1; i >= 0; i--) {
            if (i == ax) continue;
            if (++idx[i] < src->shape[i]) break;
            idx[i] = 0;
        }
    }

    if (owns_contig) numphp_ndarray_free(contig);
    return out;
}

numphp_ndarray *numphp_sort(numphp_ndarray *src, int axis)
{
    return sort_or_argsort(src, axis, 0);
}

numphp_ndarray *numphp_argsort(numphp_ndarray *src, int axis)
{
    return sort_or_argsort(src, axis, 1);
}

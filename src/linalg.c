/* Linalg — LAPACK-backed linear algebra.
 *
 * Strategy: every op materialises its 2-D inputs into an F-contiguous (column-major)
 * scratch buffer of the right dtype, hands that to LAPACK, then copies the result
 * back into a row-major NDArray. The "transpose trick" (interpret row-major bytes as
 * column-major to skip the copy) works for inv() and det() but breaks for solve()
 * with multi-RHS, svd(), and eig(). Uniform col-major copy keeps the code simple
 * and correct; performance optimization is deferred (see CHANGELOG 0.0.8).
 *
 * dtype dispatch: pure f32 inputs run on s* routines; everything else (f64, mixed,
 * integer) promotes to f64 and runs on d* routines. Mirrors Story 8 BLAS dispatch.
 */

#include "linalg.h"
#include "ndarray.h"
#include "nditer.h"
#include "lapack_names.h"

#include "Zend/zend_exceptions.h"
#include "Zend/zend_interfaces.h"

#include <math.h>
#include <string.h>
#include <stdlib.h>

/* PHP_METHOD()-generated functions (zim_Linalg_*) are referenced only via the
 * zend_function_entry table by function pointer. See decision 36. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-prototypes"

zend_class_entry *numphp_linalg_ce;

/* ============================================================================
 * Helpers
 * ============================================================================ */

/* Copy 2-D src into dst as column-major (F-contig): dst[j*m + i] = src[i, j].
 * Per dtype to avoid f32 round-tripping through double. */
static void copy_to_fcontig_f64(double *dst, numphp_ndarray *src)
{
    if (src->ndim == 1) {
        zend_long n = src->shape[0];
        char *base = (char *)src->buffer->data + src->offset;
        for (zend_long i = 0; i < n; i++) {
            dst[i] = numphp_read_f64(base + i * src->strides[0], src->dtype);
        }
        return;
    }
    zend_long m = src->shape[0], n = src->shape[1];
    char *base = (char *)src->buffer->data + src->offset;
    for (zend_long i = 0; i < m; i++) {
        for (zend_long j = 0; j < n; j++) {
            char *p = base + i * src->strides[0] + j * src->strides[1];
            dst[j * m + i] = numphp_read_f64(p, src->dtype);
        }
    }
}

static void copy_to_fcontig_f32(float *dst, numphp_ndarray *src)
{
    if (src->ndim == 1) {
        zend_long n = src->shape[0];
        char *base = (char *)src->buffer->data + src->offset;
        for (zend_long i = 0; i < n; i++) {
            dst[i] = numphp_read_f32(base + i * src->strides[0], src->dtype);
        }
        return;
    }
    zend_long m = src->shape[0], n = src->shape[1];
    char *base = (char *)src->buffer->data + src->offset;
    for (zend_long i = 0; i < m; i++) {
        for (zend_long j = 0; j < n; j++) {
            char *p = base + i * src->strides[0] + j * src->strides[1];
            dst[j * m + i] = numphp_read_f32(p, src->dtype);
        }
    }
}

/* Copy F-contig (m, n) buffer into a freshly allocated row-major NDArray. */
static numphp_ndarray *fcontig_to_ndarray_f64(const double *src, zend_long m, zend_long n)
{
    zend_long shape[2] = { m, n };
    numphp_ndarray *out = numphp_ndarray_alloc_owner(NUMPHP_FLOAT64, 2, shape);
    double *dst = (double *)out->buffer->data;
    for (zend_long i = 0; i < m; i++) {
        for (zend_long j = 0; j < n; j++) {
            dst[i * n + j] = src[j * m + i];
        }
    }
    return out;
}

static numphp_ndarray *fcontig_to_ndarray_f32(const float *src, zend_long m, zend_long n)
{
    zend_long shape[2] = { m, n };
    numphp_ndarray *out = numphp_ndarray_alloc_owner(NUMPHP_FLOAT32, 2, shape);
    float *dst = (float *)out->buffer->data;
    for (zend_long i = 0; i < m; i++) {
        for (zend_long j = 0; j < n; j++) {
            dst[i * n + j] = src[j * m + i];
        }
    }
    return out;
}

static int require_2d_square(numphp_ndarray *a, const char *opname, zend_long *n_out)
{
    if (a->ndim != 2) {
        zend_throw_exception_ex(numphp_shape_exception_ce, 0,
            "%s: input must be 2-D (got %d-D)", opname, a->ndim);
        return 0;
    }
    if (a->shape[0] != a->shape[1]) {
        zend_throw_exception_ex(numphp_shape_exception_ce, 0,
            "%s: input must be square (got %lldx%lld)",
            opname, (long long)a->shape[0], (long long)a->shape[1]);
        return 0;
    }
    *n_out = a->shape[0];
    return 1;
}

static int require_2d(numphp_ndarray *a, const char *opname)
{
    if (a->ndim != 2) {
        zend_throw_exception_ex(numphp_shape_exception_ce, 0,
            "%s: input must be 2-D (got %d-D)", opname, a->ndim);
        return 0;
    }
    return 1;
}

/* ============================================================================
 * inv(A) — dgetrf + dgetri
 * ============================================================================ */

static numphp_ndarray *linalg_inv_f64(numphp_ndarray *a, zend_long n)
{
    numphp_lapack_int N = (numphp_lapack_int)n;
    numphp_lapack_int lda = N, info = 0;
    double *A = emalloc(sizeof(double) * (size_t)(n * n));
    copy_to_fcontig_f64(A, a);

    numphp_lapack_int *ipiv = emalloc(sizeof(numphp_lapack_int) * (size_t)n);

    dgetrf_(&N, &N, A, &lda, ipiv, &info);
    if (info != 0) {
        efree(A); efree(ipiv);
        zend_throw_exception_ex(numphp_ndarray_exception_ce, 0,
            "Linalg::inv: dgetrf failed (info=%d, matrix is singular)", (int)info);
        return NULL;
    }

    /* Workspace query for dgetri */
    double work_query = 0.0;
    numphp_lapack_int lwork = -1;
    dgetri_(&N, A, &lda, ipiv, &work_query, &lwork, &info);
    if (info != 0) {
        efree(A); efree(ipiv);
        zend_throw_exception_ex(numphp_ndarray_exception_ce, 0,
            "Linalg::inv: dgetri workspace query failed (info=%d)", (int)info);
        return NULL;
    }
    lwork = (numphp_lapack_int)work_query;
    if (lwork < N) lwork = N;
    double *work = emalloc(sizeof(double) * (size_t)lwork);

    dgetri_(&N, A, &lda, ipiv, work, &lwork, &info);
    efree(work);
    efree(ipiv);
    if (info != 0) {
        efree(A);
        zend_throw_exception_ex(numphp_ndarray_exception_ce, 0,
            "Linalg::inv: dgetri failed (info=%d, matrix is singular)", (int)info);
        return NULL;
    }

    numphp_ndarray *out = fcontig_to_ndarray_f64(A, n, n);
    efree(A);
    return out;
}

static numphp_ndarray *linalg_inv_f32(numphp_ndarray *a, zend_long n)
{
    numphp_lapack_int N = (numphp_lapack_int)n, lda = N, info = 0;
    float *A = emalloc(sizeof(float) * (size_t)(n * n));
    copy_to_fcontig_f32(A, a);

    numphp_lapack_int *ipiv = emalloc(sizeof(numphp_lapack_int) * (size_t)n);
    sgetrf_(&N, &N, A, &lda, ipiv, &info);
    if (info != 0) {
        efree(A); efree(ipiv);
        zend_throw_exception_ex(numphp_ndarray_exception_ce, 0,
            "Linalg::inv: sgetrf failed (info=%d, matrix is singular)", (int)info);
        return NULL;
    }

    float work_query = 0.0f;
    numphp_lapack_int lwork = -1;
    sgetri_(&N, A, &lda, ipiv, &work_query, &lwork, &info);
    if (info != 0) {
        efree(A); efree(ipiv);
        zend_throw_exception_ex(numphp_ndarray_exception_ce, 0,
            "Linalg::inv: sgetri workspace query failed (info=%d)", (int)info);
        return NULL;
    }
    lwork = (numphp_lapack_int)work_query;
    if (lwork < N) lwork = N;
    float *work = emalloc(sizeof(float) * (size_t)lwork);

    sgetri_(&N, A, &lda, ipiv, work, &lwork, &info);
    efree(work); efree(ipiv);
    if (info != 0) {
        efree(A);
        zend_throw_exception_ex(numphp_ndarray_exception_ce, 0,
            "Linalg::inv: sgetri failed (info=%d, matrix is singular)", (int)info);
        return NULL;
    }

    numphp_ndarray *out = fcontig_to_ndarray_f32(A, n, n);
    efree(A);
    return out;
}

PHP_METHOD(Linalg, inv)
{
    zval *a_zv;
    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_OBJECT_OF_CLASS(a_zv, numphp_ndarray_ce)
    ZEND_PARSE_PARAMETERS_END();

    numphp_ndarray *a = Z_NDARRAY_P(a_zv);
    zend_long n;
    if (!require_2d_square(a, "Linalg::inv", &n)) RETURN_THROWS();

    numphp_ndarray *out = (a->dtype == NUMPHP_FLOAT32)
        ? linalg_inv_f32(a, n)
        : linalg_inv_f64(a, n);
    if (!out) RETURN_THROWS();
    numphp_zval_wrap_ndarray(return_value, out);
}

/* ============================================================================
 * det(A) — dgetrf, then product of diagonal of U with sign from pivots.
 * ============================================================================ */

static int dgetrf_pivot_sign(const numphp_lapack_int *ipiv, zend_long n)
{
    /* dgetrf returns 1-indexed pivots in IPIV. Sign of det is (-1)^k where k is
     * the number of actual swaps (i.e. ipiv[i] != i+1). */
    int sign = 1;
    for (zend_long i = 0; i < n; i++) {
        if ((zend_long)ipiv[i] != i + 1) sign = -sign;
    }
    return sign;
}

static int linalg_det_f64(numphp_ndarray *a, zend_long n, double *out)
{
    numphp_lapack_int N = (numphp_lapack_int)n, lda = N, info = 0;
    double *A = emalloc(sizeof(double) * (size_t)(n * n));
    copy_to_fcontig_f64(A, a);

    numphp_lapack_int *ipiv = emalloc(sizeof(numphp_lapack_int) * (size_t)n);
    dgetrf_(&N, &N, A, &lda, ipiv, &info);

    if (info < 0) {
        efree(A); efree(ipiv);
        zend_throw_exception_ex(numphp_ndarray_exception_ce, 0,
            "Linalg::det: dgetrf failed with invalid arg (info=%d)", (int)info);
        return 0;
    }
    if (info > 0) {
        /* U[info, info] = 0 — singular matrix. det = 0. */
        *out = 0.0;
        efree(A); efree(ipiv);
        return 1;
    }

    double det = 1.0;
    for (zend_long i = 0; i < n; i++) {
        det *= A[i * n + i];
    }
    det *= (double)dgetrf_pivot_sign(ipiv, n);
    *out = det;

    efree(A); efree(ipiv);
    return 1;
}

static int linalg_det_f32(numphp_ndarray *a, zend_long n, double *out)
{
    numphp_lapack_int N = (numphp_lapack_int)n, lda = N, info = 0;
    float *A = emalloc(sizeof(float) * (size_t)(n * n));
    copy_to_fcontig_f32(A, a);

    numphp_lapack_int *ipiv = emalloc(sizeof(numphp_lapack_int) * (size_t)n);
    sgetrf_(&N, &N, A, &lda, ipiv, &info);
    if (info < 0) {
        efree(A); efree(ipiv);
        zend_throw_exception_ex(numphp_ndarray_exception_ce, 0,
            "Linalg::det: sgetrf failed with invalid arg (info=%d)", (int)info);
        return 0;
    }
    if (info > 0) {
        *out = 0.0;
        efree(A); efree(ipiv);
        return 1;
    }

    double det = 1.0;
    for (zend_long i = 0; i < n; i++) {
        det *= (double)A[i * n + i];
    }
    det *= (double)dgetrf_pivot_sign(ipiv, n);
    *out = det;

    efree(A); efree(ipiv);
    return 1;
}

PHP_METHOD(Linalg, det)
{
    zval *a_zv;
    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_OBJECT_OF_CLASS(a_zv, numphp_ndarray_ce)
    ZEND_PARSE_PARAMETERS_END();

    numphp_ndarray *a = Z_NDARRAY_P(a_zv);
    zend_long n;
    if (!require_2d_square(a, "Linalg::det", &n)) RETURN_THROWS();

    double det = 0.0;
    int ok = (a->dtype == NUMPHP_FLOAT32)
        ? linalg_det_f32(a, n, &det)
        : linalg_det_f64(a, n, &det);
    if (!ok) RETURN_THROWS();
    RETURN_DOUBLE(det);
}

/* ============================================================================
 * solve(A, b) — dgesv. b is 1-D vector or 2-D matrix (multiple RHS).
 * ============================================================================ */

static numphp_ndarray *linalg_solve_f64(numphp_ndarray *a, numphp_ndarray *b,
                                        zend_long n, zend_long nrhs, int b_is_1d)
{
    numphp_lapack_int N = (numphp_lapack_int)n;
    numphp_lapack_int NRHS = (numphp_lapack_int)nrhs;
    numphp_lapack_int lda = N, ldb = N, info = 0;

    double *A = emalloc(sizeof(double) * (size_t)(n * n));
    double *B = emalloc(sizeof(double) * (size_t)(n * nrhs));

    copy_to_fcontig_f64(A, a);

    if (b_is_1d) {
        char *base = (char *)b->buffer->data + b->offset;
        for (zend_long i = 0; i < n; i++) {
            B[i] = numphp_read_f64(base + i * b->strides[0], b->dtype);
        }
    } else {
        copy_to_fcontig_f64(B, b);
    }

    numphp_lapack_int *ipiv = emalloc(sizeof(numphp_lapack_int) * (size_t)n);
    dgesv_(&N, &NRHS, A, &lda, ipiv, B, &ldb, &info);
    efree(A); efree(ipiv);

    if (info != 0) {
        efree(B);
        zend_throw_exception_ex(numphp_ndarray_exception_ce, 0,
            "Linalg::solve: dgesv failed (info=%d, matrix is singular)", (int)info);
        return NULL;
    }

    numphp_ndarray *out;
    if (b_is_1d) {
        zend_long shape[1] = { n };
        out = numphp_ndarray_alloc_owner(NUMPHP_FLOAT64, 1, shape);
        memcpy(out->buffer->data, B, sizeof(double) * (size_t)n);
    } else {
        out = fcontig_to_ndarray_f64(B, n, nrhs);
    }
    efree(B);
    return out;
}

static numphp_ndarray *linalg_solve_f32(numphp_ndarray *a, numphp_ndarray *b,
                                        zend_long n, zend_long nrhs, int b_is_1d)
{
    numphp_lapack_int N = (numphp_lapack_int)n;
    numphp_lapack_int NRHS = (numphp_lapack_int)nrhs;
    numphp_lapack_int lda = N, ldb = N, info = 0;

    float *A = emalloc(sizeof(float) * (size_t)(n * n));
    float *B = emalloc(sizeof(float) * (size_t)(n * nrhs));

    copy_to_fcontig_f32(A, a);
    if (b_is_1d) {
        char *base = (char *)b->buffer->data + b->offset;
        for (zend_long i = 0; i < n; i++) {
            B[i] = numphp_read_f32(base + i * b->strides[0], b->dtype);
        }
    } else {
        copy_to_fcontig_f32(B, b);
    }

    numphp_lapack_int *ipiv = emalloc(sizeof(numphp_lapack_int) * (size_t)n);
    sgesv_(&N, &NRHS, A, &lda, ipiv, B, &ldb, &info);
    efree(A); efree(ipiv);
    if (info != 0) {
        efree(B);
        zend_throw_exception_ex(numphp_ndarray_exception_ce, 0,
            "Linalg::solve: sgesv failed (info=%d, matrix is singular)", (int)info);
        return NULL;
    }

    numphp_ndarray *out;
    if (b_is_1d) {
        zend_long shape[1] = { n };
        out = numphp_ndarray_alloc_owner(NUMPHP_FLOAT32, 1, shape);
        memcpy(out->buffer->data, B, sizeof(float) * (size_t)n);
    } else {
        out = fcontig_to_ndarray_f32(B, n, nrhs);
    }
    efree(B);
    return out;
}

PHP_METHOD(Linalg, solve)
{
    zval *a_zv, *b_zv;
    ZEND_PARSE_PARAMETERS_START(2, 2)
        Z_PARAM_OBJECT_OF_CLASS(a_zv, numphp_ndarray_ce)
        Z_PARAM_OBJECT_OF_CLASS(b_zv, numphp_ndarray_ce)
    ZEND_PARSE_PARAMETERS_END();

    numphp_ndarray *a = Z_NDARRAY_P(a_zv);
    numphp_ndarray *b = Z_NDARRAY_P(b_zv);

    zend_long n;
    if (!require_2d_square(a, "Linalg::solve", &n)) RETURN_THROWS();

    int b_is_1d = (b->ndim == 1);
    zend_long nrhs;
    if (b_is_1d) {
        if (b->shape[0] != n) {
            zend_throw_exception_ex(numphp_shape_exception_ce, 0,
                "Linalg::solve: A is %lldx%lld but b has length %lld",
                (long long)n, (long long)n, (long long)b->shape[0]);
            RETURN_THROWS();
        }
        nrhs = 1;
    } else if (b->ndim == 2) {
        if (b->shape[0] != n) {
            zend_throw_exception_ex(numphp_shape_exception_ce, 0,
                "Linalg::solve: A is %lldx%lld but b has %lld rows",
                (long long)n, (long long)n, (long long)b->shape[0]);
            RETURN_THROWS();
        }
        nrhs = b->shape[1];
    } else {
        zend_throw_exception_ex(numphp_shape_exception_ce, 0,
            "Linalg::solve: b must be 1-D or 2-D (got %d-D)", b->ndim);
        RETURN_THROWS();
    }

    /* f32 only when BOTH inputs are f32 — same rule as Story 8 BLAS. */
    int both_f32 = (a->dtype == NUMPHP_FLOAT32 && b->dtype == NUMPHP_FLOAT32);

    numphp_ndarray *out = both_f32
        ? linalg_solve_f32(a, b, n, nrhs, b_is_1d)
        : linalg_solve_f64(a, b, n, nrhs, b_is_1d);
    if (!out) RETURN_THROWS();
    numphp_zval_wrap_ndarray(return_value, out);
}

/* ============================================================================
 * svd(A) — dgesdd, JOBZ='S' (thin SVD).
 *
 * For row-major input A with shape (m, n), thin SVD has:
 *   U  = (m, k), S = (k,), Vt = (k, n), where k = min(m, n).
 *
 * dgesdd is column-major; we copy A into F-contig scratch, allocate F-contig U
 * and Vt scratch, then transpose-copy U and Vt back into row-major NDArrays.
 * ============================================================================ */

static int linalg_svd_f64(numphp_ndarray *a, zend_long m, zend_long n,
                          numphp_ndarray **U_out, numphp_ndarray **S_out, numphp_ndarray **Vt_out)
{
    zend_long k = (m < n) ? m : n;
    numphp_lapack_int M = (numphp_lapack_int)m;
    numphp_lapack_int N = (numphp_lapack_int)n;
    numphp_lapack_int lda = M, ldu = M, ldvt = (numphp_lapack_int)k, info = 0;

    double *A  = emalloc(sizeof(double) * (size_t)(m * n));
    double *U  = emalloc(sizeof(double) * (size_t)(m * k));
    double *Vt = emalloc(sizeof(double) * (size_t)(k * n));
    double *S  = emalloc(sizeof(double) * (size_t)k);
    numphp_lapack_int *iwork = emalloc(sizeof(numphp_lapack_int) * (size_t)(8 * k));

    copy_to_fcontig_f64(A, a);

    char jobz = 'S';
    double wq = 0.0;
    numphp_lapack_int lwork = -1;
    dgesdd_(&jobz, &M, &N, A, &lda, S, U, &ldu, Vt, &ldvt, &wq, &lwork, iwork, &info);
    if (info != 0) {
        efree(A); efree(U); efree(Vt); efree(S); efree(iwork);
        zend_throw_exception_ex(numphp_ndarray_exception_ce, 0,
            "Linalg::svd: workspace query failed (info=%d)", (int)info);
        return 0;
    }
    lwork = (numphp_lapack_int)wq;
    double *work = emalloc(sizeof(double) * (size_t)lwork);

    dgesdd_(&jobz, &M, &N, A, &lda, S, U, &ldu, Vt, &ldvt, work, &lwork, iwork, &info);
    efree(work); efree(iwork); efree(A);

    if (info != 0) {
        efree(U); efree(Vt); efree(S);
        zend_throw_exception_ex(numphp_ndarray_exception_ce, 0,
            "Linalg::svd: dgesdd failed to converge (info=%d)", (int)info);
        return 0;
    }

    *U_out  = fcontig_to_ndarray_f64(U,  m, k);
    *Vt_out = fcontig_to_ndarray_f64(Vt, k, n);

    zend_long s_shape[1] = { k };
    *S_out = numphp_ndarray_alloc_owner(NUMPHP_FLOAT64, 1, s_shape);
    memcpy((*S_out)->buffer->data, S, sizeof(double) * (size_t)k);

    efree(U); efree(Vt); efree(S);
    return 1;
}

static int linalg_svd_f32(numphp_ndarray *a, zend_long m, zend_long n,
                          numphp_ndarray **U_out, numphp_ndarray **S_out, numphp_ndarray **Vt_out)
{
    zend_long k = (m < n) ? m : n;
    numphp_lapack_int M = (numphp_lapack_int)m, N = (numphp_lapack_int)n;
    numphp_lapack_int lda = M, ldu = M, ldvt = (numphp_lapack_int)k, info = 0;

    float *A  = emalloc(sizeof(float) * (size_t)(m * n));
    float *U  = emalloc(sizeof(float) * (size_t)(m * k));
    float *Vt = emalloc(sizeof(float) * (size_t)(k * n));
    float *S  = emalloc(sizeof(float) * (size_t)k);
    numphp_lapack_int *iwork = emalloc(sizeof(numphp_lapack_int) * (size_t)(8 * k));

    copy_to_fcontig_f32(A, a);

    char jobz = 'S';
    float wq = 0.0f;
    numphp_lapack_int lwork = -1;
    sgesdd_(&jobz, &M, &N, A, &lda, S, U, &ldu, Vt, &ldvt, &wq, &lwork, iwork, &info);
    if (info != 0) {
        efree(A); efree(U); efree(Vt); efree(S); efree(iwork);
        zend_throw_exception_ex(numphp_ndarray_exception_ce, 0,
            "Linalg::svd: workspace query failed (info=%d)", (int)info);
        return 0;
    }
    lwork = (numphp_lapack_int)wq;
    float *work = emalloc(sizeof(float) * (size_t)lwork);

    sgesdd_(&jobz, &M, &N, A, &lda, S, U, &ldu, Vt, &ldvt, work, &lwork, iwork, &info);
    efree(work); efree(iwork); efree(A);

    if (info != 0) {
        efree(U); efree(Vt); efree(S);
        zend_throw_exception_ex(numphp_ndarray_exception_ce, 0,
            "Linalg::svd: sgesdd failed to converge (info=%d)", (int)info);
        return 0;
    }

    *U_out  = fcontig_to_ndarray_f32(U,  m, k);
    *Vt_out = fcontig_to_ndarray_f32(Vt, k, n);

    zend_long s_shape[1] = { k };
    *S_out = numphp_ndarray_alloc_owner(NUMPHP_FLOAT32, 1, s_shape);
    memcpy((*S_out)->buffer->data, S, sizeof(float) * (size_t)k);

    efree(U); efree(Vt); efree(S);
    return 1;
}

PHP_METHOD(Linalg, svd)
{
    zval *a_zv;
    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_OBJECT_OF_CLASS(a_zv, numphp_ndarray_ce)
    ZEND_PARSE_PARAMETERS_END();

    numphp_ndarray *a = Z_NDARRAY_P(a_zv);
    if (!require_2d(a, "Linalg::svd")) RETURN_THROWS();

    zend_long m = a->shape[0], n = a->shape[1];

    numphp_ndarray *U = NULL, *S = NULL, *Vt = NULL;
    int ok = (a->dtype == NUMPHP_FLOAT32)
        ? linalg_svd_f32(a, m, n, &U, &S, &Vt)
        : linalg_svd_f64(a, m, n, &U, &S, &Vt);
    if (!ok) RETURN_THROWS();

    array_init(return_value);
    zval z_u, z_s, z_vt;
    numphp_zval_wrap_ndarray(&z_u,  U);
    numphp_zval_wrap_ndarray(&z_s,  S);
    numphp_zval_wrap_ndarray(&z_vt, Vt);
    add_next_index_zval(return_value, &z_u);
    add_next_index_zval(return_value, &z_s);
    add_next_index_zval(return_value, &z_vt);
}

/* ============================================================================
 * eig(A) — dgeev. Returns [w, V] where w is eigenvalues, V columns are right
 * eigenvectors. v1 throws if any eigenvalue is complex (no complex dtype yet).
 * ============================================================================ */

static int linalg_eig_f64(numphp_ndarray *a, zend_long n,
                          numphp_ndarray **w_out, numphp_ndarray **V_out)
{
    numphp_lapack_int N = (numphp_lapack_int)n, lda = N, ldvl = 1, ldvr = N, info = 0;

    double *A  = emalloc(sizeof(double) * (size_t)(n * n));
    double *wr = emalloc(sizeof(double) * (size_t)n);
    double *wi = emalloc(sizeof(double) * (size_t)n);
    double *vr = emalloc(sizeof(double) * (size_t)(n * n));
    double *vl = NULL;       /* JOBVL='N' — not used */

    copy_to_fcontig_f64(A, a);

    char jobvl = 'N', jobvr = 'V';
    double wq = 0.0;
    numphp_lapack_int lwork = -1;
    dgeev_(&jobvl, &jobvr, &N, A, &lda, wr, wi, vl, &ldvl, vr, &ldvr, &wq, &lwork, &info);
    if (info != 0) {
        efree(A); efree(wr); efree(wi); efree(vr);
        zend_throw_exception_ex(numphp_ndarray_exception_ce, 0,
            "Linalg::eig: workspace query failed (info=%d)", (int)info);
        return 0;
    }
    lwork = (numphp_lapack_int)wq;
    double *work = emalloc(sizeof(double) * (size_t)lwork);

    dgeev_(&jobvl, &jobvr, &N, A, &lda, wr, wi, vl, &ldvl, vr, &ldvr, work, &lwork, &info);
    efree(work); efree(A);
    if (info != 0) {
        efree(wr); efree(wi); efree(vr);
        zend_throw_exception_ex(numphp_ndarray_exception_ce, 0,
            "Linalg::eig: dgeev failed (info=%d)", (int)info);
        return 0;
    }

    /* Reject complex eigenvalues — v1 has no complex dtype. */
    for (zend_long i = 0; i < n; i++) {
        if (wi[i] != 0.0) {
            zend_throw_exception_ex(numphp_ndarray_exception_ce, 0,
                "Linalg::eig: input has complex eigenvalues (e.g. lambda[%lld] = %g + %gi); "
                "complex dtype is deferred to v2. For real eigenvalues only, ensure your "
                "input is symmetric (A == A^T).",
                (long long)i, wr[i], wi[i]);
            efree(wr); efree(wi); efree(vr);
            return 0;
        }
    }

    zend_long w_shape[1] = { n };
    numphp_ndarray *w = numphp_ndarray_alloc_owner(NUMPHP_FLOAT64, 1, w_shape);
    memcpy(w->buffer->data, wr, sizeof(double) * (size_t)n);

    /* VR is column-major (n, n) where each column is an eigenvector. Transpose-
     * copy to row-major so V[i, j] = i-th component of j-th eigenvector. */
    numphp_ndarray *V = fcontig_to_ndarray_f64(vr, n, n);

    *w_out = w;
    *V_out = V;
    efree(wr); efree(wi); efree(vr);
    return 1;
}

static int linalg_eig_f32(numphp_ndarray *a, zend_long n,
                          numphp_ndarray **w_out, numphp_ndarray **V_out)
{
    numphp_lapack_int N = (numphp_lapack_int)n, lda = N, ldvl = 1, ldvr = N, info = 0;

    float *A  = emalloc(sizeof(float) * (size_t)(n * n));
    float *wr = emalloc(sizeof(float) * (size_t)n);
    float *wi = emalloc(sizeof(float) * (size_t)n);
    float *vr = emalloc(sizeof(float) * (size_t)(n * n));
    float *vl = NULL;

    copy_to_fcontig_f32(A, a);

    char jobvl = 'N', jobvr = 'V';
    float wq = 0.0f;
    numphp_lapack_int lwork = -1;
    sgeev_(&jobvl, &jobvr, &N, A, &lda, wr, wi, vl, &ldvl, vr, &ldvr, &wq, &lwork, &info);
    if (info != 0) {
        efree(A); efree(wr); efree(wi); efree(vr);
        zend_throw_exception_ex(numphp_ndarray_exception_ce, 0,
            "Linalg::eig: workspace query failed (info=%d)", (int)info);
        return 0;
    }
    lwork = (numphp_lapack_int)wq;
    float *work = emalloc(sizeof(float) * (size_t)lwork);

    sgeev_(&jobvl, &jobvr, &N, A, &lda, wr, wi, vl, &ldvl, vr, &ldvr, work, &lwork, &info);
    efree(work); efree(A);
    if (info != 0) {
        efree(wr); efree(wi); efree(vr);
        zend_throw_exception_ex(numphp_ndarray_exception_ce, 0,
            "Linalg::eig: sgeev failed (info=%d)", (int)info);
        return 0;
    }

    for (zend_long i = 0; i < n; i++) {
        if (wi[i] != 0.0f) {
            zend_throw_exception_ex(numphp_ndarray_exception_ce, 0,
                "Linalg::eig: input has complex eigenvalues (e.g. lambda[%lld] = %g + %gi); "
                "complex dtype is deferred to v2.",
                (long long)i, (double)wr[i], (double)wi[i]);
            efree(wr); efree(wi); efree(vr);
            return 0;
        }
    }

    zend_long w_shape[1] = { n };
    numphp_ndarray *w = numphp_ndarray_alloc_owner(NUMPHP_FLOAT32, 1, w_shape);
    memcpy(w->buffer->data, wr, sizeof(float) * (size_t)n);

    numphp_ndarray *V = fcontig_to_ndarray_f32(vr, n, n);

    *w_out = w;
    *V_out = V;
    efree(wr); efree(wi); efree(vr);
    return 1;
}

PHP_METHOD(Linalg, eig)
{
    zval *a_zv;
    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_OBJECT_OF_CLASS(a_zv, numphp_ndarray_ce)
    ZEND_PARSE_PARAMETERS_END();

    numphp_ndarray *a = Z_NDARRAY_P(a_zv);
    zend_long n;
    if (!require_2d_square(a, "Linalg::eig", &n)) RETURN_THROWS();

    numphp_ndarray *w = NULL, *V = NULL;
    int ok = (a->dtype == NUMPHP_FLOAT32)
        ? linalg_eig_f32(a, n, &w, &V)
        : linalg_eig_f64(a, n, &w, &V);
    if (!ok) RETURN_THROWS();

    array_init(return_value);
    zval z_w, z_v;
    numphp_zval_wrap_ndarray(&z_w, w);
    numphp_zval_wrap_ndarray(&z_v, V);
    add_next_index_zval(return_value, &z_w);
    add_next_index_zval(return_value, &z_v);
}

/* ============================================================================
 * norm(A, ord=2, axis=null)
 *
 *   Vector norms (1-D input, or 2-D with axis):
 *     ord=2   → Euclidean
 *     ord=1   → sum of |x|
 *     ord=inf → max |x|
 *
 *   Matrix norms (2-D input, axis=null):
 *     ord='fro' or default → Frobenius
 *     ord=1                → max column sum
 *     ord=inf              → max row sum
 *
 *   Computed without LAPACK for now — the formulas are direct and don't benefit
 *   from dlange's BLAS dispatch at v1 sizes. dnrm2 deferred until perf matters.
 * ============================================================================ */

typedef enum {
    NUMPHP_NORM_2 = 0,
    NUMPHP_NORM_1,
    NUMPHP_NORM_INF,
    NUMPHP_NORM_FRO,
} numphp_norm_kind;

static double vector_norm(const char *base, zend_long n, zend_long stride,
                          numphp_dtype dt, numphp_norm_kind kind)
{
    if (kind == NUMPHP_NORM_2 || kind == NUMPHP_NORM_FRO) {
        double s = 0.0;
        for (zend_long i = 0; i < n; i++) {
            double v = numphp_read_f64(base + i * stride, dt);
            s += v * v;
        }
        return sqrt(s);
    }
    if (kind == NUMPHP_NORM_1) {
        double s = 0.0;
        for (zend_long i = 0; i < n; i++) {
            s += fabs(numphp_read_f64(base + i * stride, dt));
        }
        return s;
    }
    /* INF */
    double mx = 0.0;
    for (zend_long i = 0; i < n; i++) {
        double v = fabs(numphp_read_f64(base + i * stride, dt));
        if (v > mx) mx = v;
    }
    return mx;
}

static double matrix_norm(numphp_ndarray *a, numphp_norm_kind kind)
{
    zend_long m = a->shape[0], n = a->shape[1];
    char *base = (char *)a->buffer->data + a->offset;

    if (kind == NUMPHP_NORM_FRO || kind == NUMPHP_NORM_2) {
        double s = 0.0;
        for (zend_long i = 0; i < m; i++) {
            for (zend_long j = 0; j < n; j++) {
                double v = numphp_read_f64(base + i * a->strides[0] + j * a->strides[1], a->dtype);
                s += v * v;
            }
        }
        return sqrt(s);
    }
    if (kind == NUMPHP_NORM_1) {
        double mx = 0.0;
        for (zend_long j = 0; j < n; j++) {
            double s = 0.0;
            for (zend_long i = 0; i < m; i++) {
                s += fabs(numphp_read_f64(base + i * a->strides[0] + j * a->strides[1], a->dtype));
            }
            if (s > mx) mx = s;
        }
        return mx;
    }
    /* INF */
    double mx = 0.0;
    for (zend_long i = 0; i < m; i++) {
        double s = 0.0;
        for (zend_long j = 0; j < n; j++) {
            s += fabs(numphp_read_f64(base + i * a->strides[0] + j * a->strides[1], a->dtype));
        }
        if (s > mx) mx = s;
    }
    return mx;
}

PHP_METHOD(Linalg, norm)
{
    zval *a_zv, *ord_zv = NULL, *axis_zv = NULL;
    ZEND_PARSE_PARAMETERS_START(1, 3)
        Z_PARAM_OBJECT_OF_CLASS(a_zv, numphp_ndarray_ce)
        Z_PARAM_OPTIONAL
        Z_PARAM_ZVAL(ord_zv)
        Z_PARAM_ZVAL(axis_zv)
    ZEND_PARSE_PARAMETERS_END();

    numphp_ndarray *a = Z_NDARRAY_P(a_zv);
    if (a->ndim != 1 && a->ndim != 2) {
        zend_throw_exception_ex(numphp_shape_exception_ce, 0,
            "Linalg::norm: input must be 1-D or 2-D (got %d-D)", a->ndim);
        RETURN_THROWS();
    }

    /* Parse ord. */
    numphp_norm_kind kind = NUMPHP_NORM_2;
    if (ord_zv && Z_TYPE_P(ord_zv) != IS_NULL) {
        if (Z_TYPE_P(ord_zv) == IS_STRING) {
            const char *s = Z_STRVAL_P(ord_zv);
            if (strcmp(s, "fro") == 0)      kind = NUMPHP_NORM_FRO;
            else if (strcmp(s, "inf") == 0) kind = NUMPHP_NORM_INF;
            else {
                zend_throw_exception_ex(numphp_ndarray_exception_ce, 0,
                    "Linalg::norm: unsupported ord '%s' (supported: 1, 2, INF, 'fro')", s);
                RETURN_THROWS();
            }
        } else if (Z_TYPE_P(ord_zv) == IS_LONG) {
            zend_long ord = Z_LVAL_P(ord_zv);
            if (ord == 1)      kind = NUMPHP_NORM_1;
            else if (ord == 2) kind = NUMPHP_NORM_2;
            else {
                zend_throw_exception_ex(numphp_ndarray_exception_ce, 0,
                    "Linalg::norm: unsupported integer ord %lld (supported: 1, 2, INF, 'fro')",
                    (long long)ord);
                RETURN_THROWS();
            }
        } else if (Z_TYPE_P(ord_zv) == IS_DOUBLE) {
            double ord = Z_DVAL_P(ord_zv);
            if (isinf(ord) && ord > 0)      kind = NUMPHP_NORM_INF;
            else if (ord == 1.0)            kind = NUMPHP_NORM_1;
            else if (ord == 2.0)            kind = NUMPHP_NORM_2;
            else {
                zend_throw_exception_ex(numphp_ndarray_exception_ce, 0,
                    "Linalg::norm: unsupported ord %g", ord);
                RETURN_THROWS();
            }
        } else {
            zend_throw_exception(numphp_ndarray_exception_ce,
                "Linalg::norm: ord must be int, float, or string", 0);
            RETURN_THROWS();
        }
    }

    int has_axis = 0;
    int axis = 0;
    if (axis_zv && Z_TYPE_P(axis_zv) != IS_NULL) {
        if (Z_TYPE_P(axis_zv) != IS_LONG) {
            zend_throw_exception(numphp_ndarray_exception_ce,
                "Linalg::norm: axis must be int or null", 0);
            RETURN_THROWS();
        }
        has_axis = 1;
        axis = (int)Z_LVAL_P(axis_zv);
    }

    if (a->ndim == 1) {
        char *base = (char *)a->buffer->data + a->offset;
        numphp_norm_kind k = (kind == NUMPHP_NORM_FRO) ? NUMPHP_NORM_2 : kind;
        RETURN_DOUBLE(vector_norm(base, a->shape[0], a->strides[0], a->dtype, k));
    }

    /* 2-D, no axis → matrix norm */
    if (!has_axis) {
        RETURN_DOUBLE(matrix_norm(a, kind));
    }

    /* 2-D with axis → vector norm along that axis. */
    if (axis < 0) axis += a->ndim;
    if (axis < 0 || axis >= a->ndim) {
        zend_throw_exception_ex(numphp_shape_exception_ce, 0,
            "Linalg::norm: axis out of range for ndim %d", a->ndim);
        RETURN_THROWS();
    }
    int other = 1 - axis;
    zend_long out_size = a->shape[other];
    zend_long out_shape[1] = { out_size };
    numphp_dtype out_dt = (a->dtype == NUMPHP_FLOAT32) ? NUMPHP_FLOAT32 : NUMPHP_FLOAT64;
    numphp_ndarray *out = numphp_ndarray_alloc_owner(out_dt, 1, out_shape);
    char *out_base = (char *)out->buffer->data;
    char *base = (char *)a->buffer->data + a->offset;
    numphp_norm_kind k = (kind == NUMPHP_NORM_FRO) ? NUMPHP_NORM_2 : kind;

    for (zend_long o = 0; o < out_size; o++) {
        char *line_base = base + o * a->strides[other];
        double v = vector_norm(line_base, a->shape[axis], a->strides[axis], a->dtype, k);
        numphp_write_scalar_at(out_base + o * out->itemsize, out_dt, v, (zend_long)v);
    }

    numphp_zval_wrap_ndarray(return_value, out);
}

/* ============================================================================
 * Class registration
 * ============================================================================ */

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(arginfo_linalg_inv, 0, 1, NDArray, 0)
    ZEND_ARG_OBJ_INFO(0, a, NDArray, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_linalg_det, 0, 1, IS_DOUBLE, 0)
    ZEND_ARG_OBJ_INFO(0, a, NDArray, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(arginfo_linalg_solve, 0, 2, NDArray, 0)
    ZEND_ARG_OBJ_INFO(0, a, NDArray, 0)
    ZEND_ARG_OBJ_INFO(0, b, NDArray, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_linalg_svd, 0, 1, IS_ARRAY, 0)
    ZEND_ARG_OBJ_INFO(0, a, NDArray, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_linalg_eig, 0, 1, IS_ARRAY, 0)
    ZEND_ARG_OBJ_INFO(0, a, NDArray, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_linalg_norm, 0, 1, IS_MIXED, 0)
    ZEND_ARG_OBJ_INFO(0, a, NDArray, 0)
    ZEND_ARG_INFO_WITH_DEFAULT_VALUE(0, ord, "2")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, axis, IS_LONG, 1, "null")
ZEND_END_ARG_INFO()

static const zend_function_entry numphp_linalg_methods[] = {
    PHP_ME(Linalg, inv,   arginfo_linalg_inv,   ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_ME(Linalg, det,   arginfo_linalg_det,   ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_ME(Linalg, solve, arginfo_linalg_solve, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_ME(Linalg, svd,   arginfo_linalg_svd,   ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_ME(Linalg, eig,   arginfo_linalg_eig,   ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_ME(Linalg, norm,  arginfo_linalg_norm,  ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_FE_END
};

void numphp_register_linalg_class(void)
{
    zend_class_entry ce;
    INIT_CLASS_ENTRY(ce, "Linalg", numphp_linalg_methods);
    numphp_linalg_ce = zend_register_internal_class(&ce);
    numphp_linalg_ce->ce_flags |= ZEND_ACC_FINAL;
}

#pragma GCC diagnostic pop

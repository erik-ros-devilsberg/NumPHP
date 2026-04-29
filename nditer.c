#include "nditer.h"

#include "Zend/zend_exceptions.h"

numphp_dtype numphp_promote_dtype(numphp_dtype a, numphp_dtype b)
{
    /* Table from docs/system.md.
     * Indices: NUMPHP_FLOAT32=0, NUMPHP_FLOAT64=1, NUMPHP_INT32=2, NUMPHP_INT64=3 */
    static const numphp_dtype table[4][4] = {
        /*           f32             f64             i32             i64    */
        /* f32 */ { NUMPHP_FLOAT32, NUMPHP_FLOAT64, NUMPHP_FLOAT32, NUMPHP_FLOAT64 },
        /* f64 */ { NUMPHP_FLOAT64, NUMPHP_FLOAT64, NUMPHP_FLOAT64, NUMPHP_FLOAT64 },
        /* i32 */ { NUMPHP_FLOAT32, NUMPHP_FLOAT64, NUMPHP_INT32,   NUMPHP_INT64   },
        /* i64 */ { NUMPHP_FLOAT64, NUMPHP_FLOAT64, NUMPHP_INT64,   NUMPHP_INT64   },
    };
    return table[a][b];
}

int numphp_broadcast_shape(int nop, numphp_ndarray **ops, int *out_ndim, zend_long *out_shape)
{
    int max_ndim = 0;
    for (int i = 0; i < nop; i++) {
        if (ops[i]->ndim > max_ndim) max_ndim = ops[i]->ndim;
    }
    if (max_ndim > NUMPHP_MAX_NDIM) {
        zend_throw_exception(numphp_shape_exception_ce, "Too many dimensions for broadcast", 0);
        return 0;
    }

    for (int d = 0; d < max_ndim; d++) {
        zend_long dim = 1;
        for (int i = 0; i < nop; i++) {
            int pad = max_ndim - ops[i]->ndim;
            int axis = d - pad;
            zend_long s = (axis < 0) ? 1 : ops[i]->shape[axis];
            if (s == 1) continue;
            if (dim == 1) { dim = s; continue; }
            if (s != dim) {
                zend_throw_exception_ex(numphp_shape_exception_ce, 0,
                    "Shape mismatch for broadcast at axis %d (%lld vs %lld)",
                    d, (long long)dim, (long long)s);
                return 0;
            }
        }
        out_shape[d] = dim;
    }
    *out_ndim = max_ndim;
    return 1;
}

int numphp_nditer_init(numphp_nditer *it, int nop, numphp_ndarray **ops, numphp_ndarray *out)
{
    if (nop + 1 > NUMPHP_ITER_MAX_OPERANDS) {
        zend_throw_exception(numphp_shape_exception_ce, "Too many iterator operands", 0);
        return 0;
    }

    it->ndim = out->ndim;
    it->size = 1;
    it->pos = 0;
    it->done = 0;

    for (int d = 0; d < it->ndim; d++) {
        it->shape[d] = out->shape[d];
        it->size *= out->shape[d];
        it->index[d] = 0;
    }

    for (int i = 0; i < nop; i++) {
        int pad = it->ndim - ops[i]->ndim;
        for (int d = 0; d < it->ndim; d++) {
            int axis = d - pad;
            zend_long shape_d = (axis < 0) ? 1 : ops[i]->shape[axis];
            if (shape_d == 1 && it->shape[d] != 1) {
                it->strides[i][d] = 0;
            } else if (shape_d == it->shape[d]) {
                it->strides[i][d] = (axis < 0) ? 0 : ops[i]->strides[axis];
            } else {
                zend_throw_exception(numphp_shape_exception_ce,
                    "Internal error: shape mismatch in nditer_init", 0);
                return 0;
            }
        }
        it->base[i] = (char *)ops[i]->buffer->data + ops[i]->offset;
        it->ptr[i] = it->base[i];
    }

    /* Output occupies slot `nop` */
    int oi = nop;
    for (int d = 0; d < it->ndim; d++) {
        it->strides[oi][d] = out->strides[d];
    }
    it->base[oi] = (char *)out->buffer->data + out->offset;
    it->ptr[oi] = it->base[oi];

    it->nop = nop + 1;

    if (it->size == 0 || it->ndim == 0) {
        if (it->size == 0) it->done = 1;
        /* For 0-D output (size 1), one iteration is fine; ptrs already set. */
    }

    return 1;
}

void numphp_nditer_next(numphp_nditer *it)
{
    it->pos++;
    if (it->pos >= it->size) {
        it->done = 1;
        return;
    }

    /* increment from the inner-most axis; carry on overflow */
    for (int d = it->ndim - 1; d >= 0; d--) {
        if (++it->index[d] < it->shape[d]) {
            for (int i = 0; i < it->nop; i++) {
                it->ptr[i] += it->strides[i][d];
            }
            return;
        }
        it->index[d] = 0;
        for (int i = 0; i < it->nop; i++) {
            it->ptr[i] -= it->strides[i][d] * (it->shape[d] - 1);
        }
    }
}

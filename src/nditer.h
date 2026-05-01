#ifndef NUMPHP_NDITER_H
#define NUMPHP_NDITER_H

#include "ndarray.h"

#include <stdint.h>

#define NUMPHP_ITER_MAX_OPERANDS 4

/* Broadcast-aware nd-iterator.
 * Holds up to NUMPHP_ITER_MAX_OPERANDS operand pointers and stride sets;
 * the convention used by element-wise ops is: indices 0..nop-1 are inputs,
 * the next slot (nop) is the output. */
typedef struct {
    int        ndim;
    int        nop;                                                  /* total operands incl. output */
    zend_long  shape[NUMPHP_MAX_NDIM];                               /* result shape */
    zend_long  index[NUMPHP_MAX_NDIM];
    zend_long  strides[NUMPHP_ITER_MAX_OPERANDS][NUMPHP_MAX_NDIM];   /* virtual strides per operand (0 = broadcast) */
    char      *base[NUMPHP_ITER_MAX_OPERANDS];
    char      *ptr[NUMPHP_ITER_MAX_OPERANDS];
    zend_long  size;
    zend_long  pos;
    int        done;
} numphp_nditer;

int  numphp_broadcast_shape(int nop, numphp_ndarray **ops, int *out_ndim, zend_long *out_shape);
int  numphp_nditer_init(numphp_nditer *it, int nop, numphp_ndarray **ops, numphp_ndarray *out);
void numphp_nditer_next(numphp_nditer *it);

numphp_dtype numphp_promote_dtype(numphp_dtype a, numphp_dtype b);

/* typed reads at a byte pointer, given the source dtype.
 * bool reads are lenient: any non-zero byte → 1 (decision 32). */
static inline double numphp_read_f64(const char *p, numphp_dtype dt) {
    switch (dt) {
        case NUMPHP_FLOAT32: return (double)*(const float *)p;
        case NUMPHP_FLOAT64: return *(const double *)p;
        case NUMPHP_INT32:   return (double)*(const int32_t *)p;
        case NUMPHP_INT64:   return (double)*(const int64_t *)p;
        case NUMPHP_BOOL:    return (*(const uint8_t *)p) != 0 ? 1.0 : 0.0;
    }
    return 0.0;
}

static inline float numphp_read_f32(const char *p, numphp_dtype dt) {
    return (float)numphp_read_f64(p, dt);
}

static inline int64_t numphp_read_i64(const char *p, numphp_dtype dt) {
    switch (dt) {
        case NUMPHP_FLOAT32: return (int64_t)*(const float *)p;
        case NUMPHP_FLOAT64: return (int64_t)*(const double *)p;
        case NUMPHP_INT32:   return (int64_t)*(const int32_t *)p;
        case NUMPHP_INT64:   return *(const int64_t *)p;
        case NUMPHP_BOOL:    return (*(const uint8_t *)p) != 0 ? 1 : 0;
    }
    return 0;
}

#endif /* NUMPHP_NDITER_H */

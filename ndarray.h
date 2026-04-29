#ifndef NUMPHP_NDARRAY_H
#define NUMPHP_NDARRAY_H

#include "numphp.h"

#define NUMPHP_MAX_NDIM 16

#define NUMPHP_C_CONTIGUOUS  (1u << 0)
#define NUMPHP_F_CONTIGUOUS  (1u << 1)
#define NUMPHP_WRITEABLE     (1u << 2)

typedef enum {
    NUMPHP_FLOAT32 = 0,
    NUMPHP_FLOAT64 = 1,
    NUMPHP_INT32   = 2,
    NUMPHP_INT64   = 3
} numphp_dtype;

typedef struct {
    void      *data;
    zend_long  nbytes;
    zend_long  refcount;
} numphp_buffer;

typedef struct {
    numphp_buffer *buffer;
    zend_long     *shape;
    zend_long     *strides;
    zend_long      offset;
    int            ndim;
    numphp_dtype   dtype;
    zend_long      itemsize;
    zend_long      size;
    unsigned       flags;
} numphp_ndarray;

typedef struct {
    numphp_ndarray *array;
    zend_object     std;
} numphp_ndarray_object;

extern zend_class_entry *numphp_ndarray_ce;

void numphp_register_ndarray_class(void);

/* buffer lifecycle */
numphp_buffer *numphp_buffer_alloc(zend_long nbytes);
void           numphp_buffer_addref(numphp_buffer *buf);
void           numphp_buffer_release(numphp_buffer *buf);

/* dtype helpers */
zend_long      numphp_dtype_size(numphp_dtype dt);
const char    *numphp_dtype_name(numphp_dtype dt);
int            numphp_dtype_from_name(const char *name, size_t len, numphp_dtype *out);

/* ndarray lifecycle */
numphp_ndarray *numphp_ndarray_alloc_owner(numphp_dtype dt, int ndim, const zend_long *shape);
void            numphp_ndarray_free(numphp_ndarray *a);
void            numphp_ndarray_recompute_contiguity(numphp_ndarray *a);

/* Typed scalar load/store at a byte pointer. Promoted from ndarray.c for use by
 * the reduction / element-wise / sort kernels in ops.c. */
void numphp_read_scalar_at(const char *p, numphp_dtype dt, double *dv_out, zend_long *lv_out);
void numphp_write_scalar_at(char *p, numphp_dtype dt, double dv, zend_long lv);

/* Allocate a fresh C-contiguous owner and copy `src` into it (any layout). */
numphp_ndarray *numphp_materialize_contiguous(numphp_ndarray *src);

/* If `src` is already C-contiguous AND already in `target_dt`, returns src and sets
 * *out_owned = 0. Otherwise allocates a fresh C-contiguous owner with values cast
 * to target_dt and sets *out_owned = 1 (caller must free). */
numphp_ndarray *numphp_ensure_contig_dtype(numphp_ndarray *src, numphp_dtype target_dt, int *out_owned);

static inline numphp_ndarray_object *numphp_obj_from_zo(zend_object *zo) {
    return (numphp_ndarray_object *)((char *)zo - XtOffsetOf(numphp_ndarray_object, std));
}

#define Z_NDARRAY_P(zv) (numphp_obj_from_zo(Z_OBJ_P(zv))->array)

void numphp_zval_wrap_ndarray(zval *out, numphp_ndarray *a);

#endif /* NUMPHP_NDARRAY_H */

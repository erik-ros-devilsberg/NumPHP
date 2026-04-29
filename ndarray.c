#include "ndarray.h"
#include "nditer.h"
#include "ops.h"

#include "Zend/zend_exceptions.h"
#include "Zend/zend_interfaces.h"

#include <cblas.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include <limits.h>

zend_class_entry *numphp_ndarray_ce;
static zend_object_handlers numphp_ndarray_handlers;

/* ===== dtype helpers ===== */

zend_long numphp_dtype_size(numphp_dtype dt)
{
    switch (dt) {
        case NUMPHP_FLOAT32: return 4;
        case NUMPHP_FLOAT64: return 8;
        case NUMPHP_INT32:   return 4;
        case NUMPHP_INT64:   return 8;
    }
    return 0;
}

const char *numphp_dtype_name(numphp_dtype dt)
{
    switch (dt) {
        case NUMPHP_FLOAT32: return "float32";
        case NUMPHP_FLOAT64: return "float64";
        case NUMPHP_INT32:   return "int32";
        case NUMPHP_INT64:   return "int64";
    }
    return "unknown";
}

int numphp_dtype_from_name(const char *name, size_t len, numphp_dtype *out)
{
    if (len == 7 && memcmp(name, "float32", 7) == 0) { *out = NUMPHP_FLOAT32; return 1; }
    if (len == 7 && memcmp(name, "float64", 7) == 0) { *out = NUMPHP_FLOAT64; return 1; }
    if (len == 5 && memcmp(name, "int32", 5) == 0)   { *out = NUMPHP_INT32;   return 1; }
    if (len == 5 && memcmp(name, "int64", 5) == 0)   { *out = NUMPHP_INT64;   return 1; }
    return 0;
}

/* ===== buffer ===== */

numphp_buffer *numphp_buffer_alloc(zend_long nbytes)
{
    numphp_buffer *buf = emalloc(sizeof(numphp_buffer));
    buf->data = (nbytes > 0) ? emalloc((size_t)nbytes) : NULL;
    buf->nbytes = nbytes;
    buf->refcount = 1;
    return buf;
}

void numphp_buffer_addref(numphp_buffer *buf)
{
    if (buf) buf->refcount++;
}

void numphp_buffer_release(numphp_buffer *buf)
{
    if (!buf) return;
    if (--buf->refcount == 0) {
        if (buf->data) efree(buf->data);
        efree(buf);
    }
}

/* ===== ndarray ===== */

void numphp_ndarray_recompute_contiguity(numphp_ndarray *a)
{
    a->flags &= ~(NUMPHP_C_CONTIGUOUS | NUMPHP_F_CONTIGUOUS);

    if (a->ndim == 0 || a->size == 0) {
        a->flags |= NUMPHP_C_CONTIGUOUS | NUMPHP_F_CONTIGUOUS;
        return;
    }

    int c_contig = 1;
    {
        zend_long expected = a->itemsize;
        for (int i = a->ndim - 1; i >= 0; i--) {
            if (a->shape[i] == 1) continue;
            if (a->strides[i] != expected) { c_contig = 0; break; }
            expected *= a->shape[i];
        }
    }
    if (c_contig) a->flags |= NUMPHP_C_CONTIGUOUS;

    int f_contig = 1;
    {
        zend_long expected = a->itemsize;
        for (int i = 0; i < a->ndim; i++) {
            if (a->shape[i] == 1) continue;
            if (a->strides[i] != expected) { f_contig = 0; break; }
            expected *= a->shape[i];
        }
    }
    if (f_contig) a->flags |= NUMPHP_F_CONTIGUOUS;
}

numphp_ndarray *numphp_ndarray_alloc_owner(numphp_dtype dt, int ndim, const zend_long *shape)
{
    numphp_ndarray *a = emalloc(sizeof(numphp_ndarray));
    a->dtype = dt;
    a->itemsize = numphp_dtype_size(dt);
    a->ndim = ndim;
    a->offset = 0;
    a->flags = NUMPHP_WRITEABLE;

    a->shape = (ndim > 0) ? emalloc(sizeof(zend_long) * ndim) : NULL;
    a->strides = (ndim > 0) ? emalloc(sizeof(zend_long) * ndim) : NULL;

    zend_long size = 1;
    for (int i = 0; i < ndim; i++) {
        a->shape[i] = shape[i];
        size *= shape[i];
    }
    a->size = size;

    if (ndim > 0) {
        a->strides[ndim - 1] = a->itemsize;
        for (int i = ndim - 2; i >= 0; i--) {
            a->strides[i] = a->strides[i + 1] * a->shape[i + 1];
        }
    }

    a->buffer = numphp_buffer_alloc(size * a->itemsize);

    numphp_ndarray_recompute_contiguity(a);
    return a;
}

void numphp_ndarray_free(numphp_ndarray *a)
{
    if (!a) return;
    numphp_buffer_release(a->buffer);
    if (a->shape) efree(a->shape);
    if (a->strides) efree(a->strides);
    efree(a);
}

static numphp_ndarray *numphp_ndarray_alloc_view(numphp_ndarray *parent,
    int new_ndim, const zend_long *new_shape, const zend_long *new_strides, zend_long new_offset)
{
    numphp_ndarray *v = emalloc(sizeof(numphp_ndarray));
    v->buffer = parent->buffer;
    numphp_buffer_addref(v->buffer);
    v->dtype = parent->dtype;
    v->itemsize = parent->itemsize;
    v->ndim = new_ndim;
    v->offset = new_offset;
    v->flags = (parent->flags & NUMPHP_WRITEABLE);

    if (new_ndim > 0) {
        v->shape = emalloc(sizeof(zend_long) * new_ndim);
        v->strides = emalloc(sizeof(zend_long) * new_ndim);
        zend_long sz = 1;
        for (int i = 0; i < new_ndim; i++) {
            v->shape[i] = new_shape[i];
            v->strides[i] = new_strides[i];
            sz *= new_shape[i];
        }
        v->size = sz;
    } else {
        v->shape = NULL;
        v->strides = NULL;
        v->size = 1;
    }

    numphp_ndarray_recompute_contiguity(v);
    return v;
}

/* ===== Zend object handlers ===== */

static zend_object *numphp_ndarray_create_object(zend_class_entry *ce)
{
    numphp_ndarray_object *intern = zend_object_alloc(sizeof(numphp_ndarray_object), ce);
    zend_object_std_init(&intern->std, ce);
    object_properties_init(&intern->std, ce);
    intern->std.handlers = &numphp_ndarray_handlers;
    intern->array = NULL;
    return &intern->std;
}

static void numphp_ndarray_free_object(zend_object *zo)
{
    numphp_ndarray_object *intern = numphp_obj_from_zo(zo);
    if (intern->array) {
        numphp_ndarray_free(intern->array);
        intern->array = NULL;
    }
    zend_object_std_dtor(&intern->std);
}

static zend_object *numphp_ndarray_clone_object(zend_object *zo)
{
    numphp_ndarray_object *src = numphp_obj_from_zo(zo);
    zend_object *new_zo = numphp_ndarray_create_object(zo->ce);
    numphp_ndarray_object *dst = numphp_obj_from_zo(new_zo);

    if (src->array) {
        numphp_ndarray *sa = src->array;
        numphp_ndarray *da = numphp_ndarray_alloc_owner(sa->dtype, sa->ndim, sa->shape);

        if (sa->size > 0) {
            char *src_base = (char *)sa->buffer->data + sa->offset;
            char *dst_base = (char *)da->buffer->data;
            zend_long itemsize = sa->itemsize;
            zend_long idx[NUMPHP_MAX_NDIM] = {0};

            for (zend_long n = 0; n < sa->size; n++) {
                zend_long src_off = 0, dst_off = 0;
                for (int i = 0; i < sa->ndim; i++) {
                    src_off += idx[i] * sa->strides[i];
                    dst_off += idx[i] * da->strides[i];
                }
                memcpy(dst_base + dst_off, src_base + src_off, (size_t)itemsize);
                for (int i = sa->ndim - 1; i >= 0; i--) {
                    if (++idx[i] < sa->shape[i]) break;
                    idx[i] = 0;
                }
            }
        }

        dst->array = da;
    }

    zend_objects_clone_members(&dst->std, &src->std);
    return new_zo;
}

void numphp_zval_wrap_ndarray(zval *out, numphp_ndarray *a)
{
    object_init_ex(out, numphp_ndarray_ce);
    numphp_ndarray_object *intern = numphp_obj_from_zo(Z_OBJ_P(out));
    intern->array = a;
}

/* ===== creation helpers ===== */

static int parse_shape(zval *shape_arr, zend_long *shape_out, int *ndim_out, zend_long *size_out)
{
    if (Z_TYPE_P(shape_arr) != IS_ARRAY) {
        zend_throw_exception(numphp_shape_exception_ce, "Shape must be an array of integers", 0);
        return 0;
    }
    HashTable *ht = Z_ARRVAL_P(shape_arr);
    int n = (int)zend_hash_num_elements(ht);
    if (n > NUMPHP_MAX_NDIM) {
        zend_throw_exception_ex(numphp_shape_exception_ce, 0,
            "Too many dimensions (%d, max %d)", n, NUMPHP_MAX_NDIM);
        return 0;
    }

    int i = 0;
    zval *v;
    zend_long size = 1;
    ZEND_HASH_FOREACH_VAL(ht, v) {
        ZVAL_DEREF(v);
        if (Z_TYPE_P(v) != IS_LONG) {
            zend_throw_exception(numphp_shape_exception_ce, "Shape entries must be integers", 0);
            return 0;
        }
        zend_long d = Z_LVAL_P(v);
        if (d < 0) {
            zend_throw_exception(numphp_shape_exception_ce, "Shape entries must be non-negative", 0);
            return 0;
        }
        shape_out[i++] = d;
        size *= d;
    } ZEND_HASH_FOREACH_END();

    *ndim_out = n;
    *size_out = size;
    return 1;
}

static int parse_dtype_str(zend_string *name, numphp_dtype *out)
{
    if (!numphp_dtype_from_name(ZSTR_VAL(name), ZSTR_LEN(name), out)) {
        zend_throw_exception_ex(numphp_dtype_exception_ce, 0,
            "Unsupported dtype: %s", ZSTR_VAL(name));
        return 0;
    }
    return 1;
}

static void zval_to_numeric(zval *value, double *dv_out, zend_long *lv_out)
{
    if (Z_TYPE_P(value) == IS_LONG) {
        *lv_out = Z_LVAL_P(value);
        *dv_out = (double)Z_LVAL_P(value);
    } else if (Z_TYPE_P(value) == IS_DOUBLE) {
        *dv_out = Z_DVAL_P(value);
        *lv_out = (zend_long)Z_DVAL_P(value);
    } else if (Z_TYPE_P(value) == IS_TRUE) {
        *lv_out = 1; *dv_out = 1.0;
    } else if (Z_TYPE_P(value) == IS_FALSE || Z_TYPE_P(value) == IS_NULL) {
        *lv_out = 0; *dv_out = 0.0;
    } else {
        zval tmp;
        ZVAL_COPY(&tmp, value);
        convert_to_double(&tmp);
        *dv_out = Z_DVAL(tmp);
        *lv_out = (zend_long)Z_DVAL(tmp);
        zval_ptr_dtor(&tmp);
    }
}

static void fill_typed(void *buf, zend_long size, numphp_dtype dt, zval *value)
{
    double dv;
    zend_long lv;
    zval_to_numeric(value, &dv, &lv);

    switch (dt) {
        case NUMPHP_FLOAT32: {
            float v = (float)dv;
            float *p = (float *)buf;
            for (zend_long i = 0; i < size; i++) p[i] = v;
            break;
        }
        case NUMPHP_FLOAT64: {
            double *p = (double *)buf;
            for (zend_long i = 0; i < size; i++) p[i] = dv;
            break;
        }
        case NUMPHP_INT32: {
            int32_t v = (int32_t)lv;
            int32_t *p = (int32_t *)buf;
            for (zend_long i = 0; i < size; i++) p[i] = v;
            break;
        }
        case NUMPHP_INT64: {
            int64_t v = (int64_t)lv;
            int64_t *p = (int64_t *)buf;
            for (zend_long i = 0; i < size; i++) p[i] = v;
            break;
        }
    }
}

/* ===== creation methods ===== */

PHP_METHOD(NDArray, zeros)
{
    zval *shape_arr;
    zend_string *dtype_name = NULL;

    ZEND_PARSE_PARAMETERS_START(1, 2)
        Z_PARAM_ARRAY(shape_arr)
        Z_PARAM_OPTIONAL
        Z_PARAM_STR(dtype_name)
    ZEND_PARSE_PARAMETERS_END();

    numphp_dtype dt = NUMPHP_FLOAT64;
    if (dtype_name && !parse_dtype_str(dtype_name, &dt)) RETURN_THROWS();

    zend_long shape[NUMPHP_MAX_NDIM];
    int ndim;
    zend_long size;
    if (!parse_shape(shape_arr, shape, &ndim, &size)) RETURN_THROWS();

    numphp_ndarray *a = numphp_ndarray_alloc_owner(dt, ndim, shape);
    if (a->buffer->data && a->buffer->nbytes > 0) {
        memset(a->buffer->data, 0, (size_t)a->buffer->nbytes);
    }
    numphp_zval_wrap_ndarray(return_value, a);
}

PHP_METHOD(NDArray, ones)
{
    zval *shape_arr;
    zend_string *dtype_name = NULL;

    ZEND_PARSE_PARAMETERS_START(1, 2)
        Z_PARAM_ARRAY(shape_arr)
        Z_PARAM_OPTIONAL
        Z_PARAM_STR(dtype_name)
    ZEND_PARSE_PARAMETERS_END();

    numphp_dtype dt = NUMPHP_FLOAT64;
    if (dtype_name && !parse_dtype_str(dtype_name, &dt)) RETURN_THROWS();

    zend_long shape[NUMPHP_MAX_NDIM];
    int ndim;
    zend_long size;
    if (!parse_shape(shape_arr, shape, &ndim, &size)) RETURN_THROWS();

    numphp_ndarray *a = numphp_ndarray_alloc_owner(dt, ndim, shape);
    if (a->size > 0) {
        zval one;
        ZVAL_LONG(&one, 1);
        fill_typed(a->buffer->data, a->size, dt, &one);
    }
    numphp_zval_wrap_ndarray(return_value, a);
}

PHP_METHOD(NDArray, full)
{
    zval *shape_arr;
    zval *value;
    zend_string *dtype_name = NULL;

    ZEND_PARSE_PARAMETERS_START(2, 3)
        Z_PARAM_ARRAY(shape_arr)
        Z_PARAM_ZVAL(value)
        Z_PARAM_OPTIONAL
        Z_PARAM_STR(dtype_name)
    ZEND_PARSE_PARAMETERS_END();

    numphp_dtype dt = NUMPHP_FLOAT64;
    if (dtype_name && !parse_dtype_str(dtype_name, &dt)) RETURN_THROWS();

    zend_long shape[NUMPHP_MAX_NDIM];
    int ndim;
    zend_long size;
    if (!parse_shape(shape_arr, shape, &ndim, &size)) RETURN_THROWS();

    numphp_ndarray *a = numphp_ndarray_alloc_owner(dt, ndim, shape);
    if (a->size > 0) {
        fill_typed(a->buffer->data, a->size, dt, value);
    }
    numphp_zval_wrap_ndarray(return_value, a);
}

PHP_METHOD(NDArray, eye)
{
    zend_long n;
    zend_long m = 0;
    zend_bool m_is_null = 1;
    zend_long k = 0;
    zend_string *dtype_name = NULL;

    ZEND_PARSE_PARAMETERS_START(1, 4)
        Z_PARAM_LONG(n)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG_OR_NULL(m, m_is_null)
        Z_PARAM_LONG(k)
        Z_PARAM_STR(dtype_name)
    ZEND_PARSE_PARAMETERS_END();

    if (m_is_null) m = n;

    if (n < 0 || m < 0) {
        zend_throw_exception(numphp_shape_exception_ce,
            "eye: dimensions must be non-negative", 0);
        RETURN_THROWS();
    }

    numphp_dtype dt = NUMPHP_FLOAT64;
    if (dtype_name && !parse_dtype_str(dtype_name, &dt)) RETURN_THROWS();

    zend_long shape[2] = { n, m };
    numphp_ndarray *a = numphp_ndarray_alloc_owner(dt, 2, shape);
    if (a->buffer->data && a->buffer->nbytes > 0) {
        memset(a->buffer->data, 0, (size_t)a->buffer->nbytes);
    }

    char *base = (char *)a->buffer->data;
    zend_long row_stride = (n > 0) ? a->strides[0] : 0;
    zend_long col_stride = (m > 0) ? a->strides[1] : 0;

    for (zend_long i = 0; i < n; i++) {
        zend_long j = i + k;
        if (j < 0 || j >= m) continue;
        char *p = base + i * row_stride + j * col_stride;
        switch (dt) {
            case NUMPHP_FLOAT32: *(float *)p   = 1.0f; break;
            case NUMPHP_FLOAT64: *(double *)p  = 1.0;  break;
            case NUMPHP_INT32:   *(int32_t *)p = 1;    break;
            case NUMPHP_INT64:   *(int64_t *)p = 1;    break;
        }
    }

    numphp_zval_wrap_ndarray(return_value, a);
}

PHP_METHOD(NDArray, arange)
{
    zval *start_z, *stop_z;
    zval *step_z = NULL;
    zend_string *dtype_name = NULL;

    ZEND_PARSE_PARAMETERS_START(2, 4)
        Z_PARAM_ZVAL(start_z)
        Z_PARAM_ZVAL(stop_z)
        Z_PARAM_OPTIONAL
        Z_PARAM_ZVAL(step_z)
        Z_PARAM_STR_OR_NULL(dtype_name)
    ZEND_PARSE_PARAMETERS_END();

    int all_int = (Z_TYPE_P(start_z) == IS_LONG)
                && (Z_TYPE_P(stop_z) == IS_LONG)
                && (step_z == NULL || Z_TYPE_P(step_z) == IS_LONG);

    numphp_dtype dt;
    if (dtype_name) {
        if (!parse_dtype_str(dtype_name, &dt)) RETURN_THROWS();
    } else {
        dt = all_int ? NUMPHP_INT64 : NUMPHP_FLOAT64;
    }

    double start_d, stop_d;
    zend_long lv;
    zval_to_numeric(start_z, &start_d, &lv);
    zval_to_numeric(stop_z,  &stop_d,  &lv);

    double step_d = 1.0;
    if (step_z) zval_to_numeric(step_z, &step_d, &lv);

    if (step_d == 0.0) {
        zend_throw_exception(numphp_ndarray_exception_ce,
            "arange: step cannot be zero", 0);
        RETURN_THROWS();
    }

    double dist = stop_d - start_d;
    zend_long count;
    if ((dist > 0 && step_d < 0) || (dist < 0 && step_d > 0) || dist == 0) {
        count = 0;
    } else {
        double c = ceil(dist / step_d);
        count = (zend_long)c;
        if (count < 0) count = 0;
    }

    zend_long shape[1] = { count };
    numphp_ndarray *a = numphp_ndarray_alloc_owner(dt, 1, shape);

    void *buf = a->buffer->data;
    for (zend_long i = 0; i < count; i++) {
        double v = start_d + step_d * (double)i;
        switch (dt) {
            case NUMPHP_FLOAT32: ((float *)buf)[i]   = (float)v;     break;
            case NUMPHP_FLOAT64: ((double *)buf)[i]  = v;            break;
            case NUMPHP_INT32:   ((int32_t *)buf)[i] = (int32_t)v;   break;
            case NUMPHP_INT64:   ((int64_t *)buf)[i] = (int64_t)v;   break;
        }
    }

    numphp_zval_wrap_ndarray(return_value, a);
}

/* fromArray: two-pass — infer shape + has_float, then fill */

static int fromarray_walk(zval *node, int depth, zend_long *shape, int *ndim_out, int *has_float_out)
{
    if (depth >= NUMPHP_MAX_NDIM) {
        zend_throw_exception(numphp_shape_exception_ce, "Array nesting too deep", 0);
        return 0;
    }
    if (Z_TYPE_P(node) == IS_ARRAY) {
        HashTable *ht = Z_ARRVAL_P(node);
        zend_long n = (zend_long)zend_hash_num_elements(ht);

        if (depth >= *ndim_out) {
            shape[depth] = n;
            *ndim_out = depth + 1;
        } else if (shape[depth] != n) {
            zend_throw_exception(numphp_shape_exception_ce,
                "Ragged array: inconsistent dimension lengths", 0);
            return 0;
        }

        zval *v;
        ZEND_HASH_FOREACH_VAL(ht, v) {
            ZVAL_DEREF(v);
            if (!fromarray_walk(v, depth + 1, shape, ndim_out, has_float_out)) return 0;
        } ZEND_HASH_FOREACH_END();
    } else {
        if (depth != *ndim_out) {
            zend_throw_exception(numphp_shape_exception_ce,
                "Ragged array: scalar at non-leaf depth", 0);
            return 0;
        }
        if (Z_TYPE_P(node) == IS_DOUBLE) *has_float_out = 1;
    }
    return 1;
}

static void fromarray_fill(zval *node, int depth, int ndim, char **out_ptr, numphp_dtype dt)
{
    if (depth == ndim) {
        double dv;
        zend_long lv;
        zval_to_numeric(node, &dv, &lv);
        switch (dt) {
            case NUMPHP_FLOAT32: *(float *)*out_ptr   = (float)dv;     *out_ptr += 4; break;
            case NUMPHP_FLOAT64: *(double *)*out_ptr  = dv;            *out_ptr += 8; break;
            case NUMPHP_INT32:   *(int32_t *)*out_ptr = (int32_t)lv;   *out_ptr += 4; break;
            case NUMPHP_INT64:   *(int64_t *)*out_ptr = (int64_t)lv;   *out_ptr += 8; break;
        }
        return;
    }
    HashTable *ht = Z_ARRVAL_P(node);
    zval *v;
    ZEND_HASH_FOREACH_VAL(ht, v) {
        ZVAL_DEREF(v);
        fromarray_fill(v, depth + 1, ndim, out_ptr, dt);
    } ZEND_HASH_FOREACH_END();
}

PHP_METHOD(NDArray, fromArray)
{
    zval *data;
    zend_string *dtype_name = NULL;

    ZEND_PARSE_PARAMETERS_START(1, 2)
        Z_PARAM_ARRAY(data)
        Z_PARAM_OPTIONAL
        Z_PARAM_STR_OR_NULL(dtype_name)
    ZEND_PARSE_PARAMETERS_END();

    zend_long shape[NUMPHP_MAX_NDIM] = {0};
    int ndim = 0;
    int has_float = 0;

    if (!fromarray_walk(data, 0, shape, &ndim, &has_float)) RETURN_THROWS();

    numphp_dtype dt;
    if (dtype_name) {
        if (!parse_dtype_str(dtype_name, &dt)) RETURN_THROWS();
    } else {
        dt = has_float ? NUMPHP_FLOAT64 : NUMPHP_INT64;
    }

    numphp_ndarray *a = numphp_ndarray_alloc_owner(dt, ndim, shape);
    char *out = (char *)a->buffer->data;
    fromarray_fill(data, 0, ndim, &out, dt);

    numphp_zval_wrap_ndarray(return_value, a);
}

/* ===== metadata accessors ===== */

PHP_METHOD(NDArray, shape)
{
    ZEND_PARSE_PARAMETERS_NONE();
    numphp_ndarray *a = Z_NDARRAY_P(ZEND_THIS);
    array_init(return_value);
    for (int i = 0; i < a->ndim; i++) {
        add_next_index_long(return_value, a->shape[i]);
    }
}

PHP_METHOD(NDArray, dtype)
{
    ZEND_PARSE_PARAMETERS_NONE();
    numphp_ndarray *a = Z_NDARRAY_P(ZEND_THIS);
    RETURN_STRING(numphp_dtype_name(a->dtype));
}

PHP_METHOD(NDArray, size)
{
    ZEND_PARSE_PARAMETERS_NONE();
    numphp_ndarray *a = Z_NDARRAY_P(ZEND_THIS);
    RETURN_LONG(a->size);
}

PHP_METHOD(NDArray, ndim)
{
    ZEND_PARSE_PARAMETERS_NONE();
    numphp_ndarray *a = Z_NDARRAY_P(ZEND_THIS);
    RETURN_LONG(a->ndim);
}

/* ===== toArray ===== */

static void toarray_recursive(numphp_ndarray *a, int dim, zend_long offset, zval *out)
{
    if (dim == a->ndim) {
        char *p = (char *)a->buffer->data + offset;
        switch (a->dtype) {
            case NUMPHP_FLOAT32: ZVAL_DOUBLE(out, *(float *)p);   break;
            case NUMPHP_FLOAT64: ZVAL_DOUBLE(out, *(double *)p);  break;
            case NUMPHP_INT32:   ZVAL_LONG(out, *(int32_t *)p);   break;
            case NUMPHP_INT64:   ZVAL_LONG(out, *(int64_t *)p);   break;
        }
        return;
    }
    array_init(out);
    for (zend_long i = 0; i < a->shape[dim]; i++) {
        zval child;
        toarray_recursive(a, dim + 1, offset + i * a->strides[dim], &child);
        add_next_index_zval(out, &child);
    }
}

PHP_METHOD(NDArray, toArray)
{
    ZEND_PARSE_PARAMETERS_NONE();
    numphp_ndarray *a = Z_NDARRAY_P(ZEND_THIS);
    if (a->ndim == 0) {
        char *p = (char *)a->buffer->data + a->offset;
        switch (a->dtype) {
            case NUMPHP_FLOAT32: RETURN_DOUBLE(*(float *)p);
            case NUMPHP_FLOAT64: RETURN_DOUBLE(*(double *)p);
            case NUMPHP_INT32:   RETURN_LONG(*(int32_t *)p);
            case NUMPHP_INT64:   RETURN_LONG(*(int64_t *)p);
        }
    }
    toarray_recursive(a, 0, a->offset, return_value);
}

/* ===== indexing / slicing helpers ===== */

static int normalize_axis0_index(numphp_ndarray *a, zval *offset, zend_long *out)
{
    if (Z_TYPE_P(offset) != IS_LONG) {
        zend_throw_exception(numphp_index_exception_ce, "Index must be an integer", 0);
        return 0;
    }
    if (a->ndim == 0) {
        zend_throw_exception(numphp_index_exception_ce, "Cannot index a 0-D array", 0);
        return 0;
    }
    zend_long idx = Z_LVAL_P(offset);
    zend_long n = a->shape[0];
    if (idx < 0) idx += n;
    if (idx < 0 || idx >= n) {
        zend_throw_exception_ex(numphp_index_exception_ce, 0,
            "Index %lld out of bounds for axis 0 with size %lld",
            (long long)Z_LVAL_P(offset), (long long)n);
        return 0;
    }
    *out = idx;
    return 1;
}

void numphp_write_scalar_at(char *p, numphp_dtype dt, double dv, zend_long lv)
{
    switch (dt) {
        case NUMPHP_FLOAT32: *(float *)p   = (float)dv;     break;
        case NUMPHP_FLOAT64: *(double *)p  = dv;            break;
        case NUMPHP_INT32:   *(int32_t *)p = (int32_t)lv;   break;
        case NUMPHP_INT64:   *(int64_t *)p = (int64_t)lv;   break;
    }
}

void numphp_read_scalar_at(const char *p, numphp_dtype dt, double *dv_out, zend_long *lv_out)
{
    switch (dt) {
        case NUMPHP_FLOAT32: *dv_out = (double)*(const float *)p;   *lv_out = (zend_long)*dv_out; break;
        case NUMPHP_FLOAT64: *dv_out = *(const double *)p;           *lv_out = (zend_long)*dv_out; break;
        case NUMPHP_INT32:   *lv_out = *(const int32_t *)p;          *dv_out = (double)*lv_out;    break;
        case NUMPHP_INT64:   *lv_out = *(const int64_t *)p;          *dv_out = (double)*lv_out;    break;
    }
}

/* Local aliases so the rest of this TU doesn't need to be sed'd. */
#define write_scalar_at numphp_write_scalar_at
#define read_scalar_at  numphp_read_scalar_at

/* ===== ArrayAccess + Countable handlers ===== */

static zval *numphp_offset_get(zend_object *zo, zval *offset, int type, zval *rv)
{
    (void)type;
    numphp_ndarray *a = numphp_obj_from_zo(zo)->array;

    zend_long idx;
    if (!normalize_axis0_index(a, offset, &idx)) {
        ZVAL_NULL(rv);
        return rv;
    }

    zend_long new_offset = a->offset + idx * a->strides[0];

    if (a->ndim == 1) {
        char *p = (char *)a->buffer->data + new_offset;
        switch (a->dtype) {
            case NUMPHP_FLOAT32: ZVAL_DOUBLE(rv, *(float *)p);  break;
            case NUMPHP_FLOAT64: ZVAL_DOUBLE(rv, *(double *)p); break;
            case NUMPHP_INT32:   ZVAL_LONG(rv, *(int32_t *)p);  break;
            case NUMPHP_INT64:   ZVAL_LONG(rv, *(int64_t *)p);  break;
        }
        return rv;
    }

    numphp_ndarray *view = numphp_ndarray_alloc_view(a,
        a->ndim - 1, a->shape + 1, a->strides + 1, new_offset);
    numphp_zval_wrap_ndarray(rv, view);
    return rv;
}

static void numphp_offset_set(zend_object *zo, zval *offset, zval *value)
{
    numphp_ndarray *a = numphp_obj_from_zo(zo)->array;

    if (!(a->flags & NUMPHP_WRITEABLE)) {
        zend_throw_exception(numphp_ndarray_exception_ce, "Array is read-only", 0);
        return;
    }
    if (offset == NULL || Z_TYPE_P(offset) == IS_NULL) {
        zend_throw_exception(numphp_index_exception_ce,
            "Appending to NDArray ($a[] = $x) is not supported", 0);
        return;
    }

    zend_long idx;
    if (!normalize_axis0_index(a, offset, &idx)) return;

    zend_long target_offset = a->offset + idx * a->strides[0];

    /* 1D scalar write */
    if (a->ndim == 1) {
        double dv;
        zend_long lv;
        zval_to_numeric(value, &dv, &lv);
        write_scalar_at((char *)a->buffer->data + target_offset, a->dtype, dv, lv);
        return;
    }

    int sub_ndim = a->ndim - 1;
    zend_long *sub_shape = a->shape + 1;
    zend_long *sub_strides = a->strides + 1;
    zend_long sub_size = 1;
    for (int i = 0; i < sub_ndim; i++) sub_size *= sub_shape[i];

    /* Scalar broadcast over sub-block */
    if (Z_TYPE_P(value) == IS_LONG  || Z_TYPE_P(value) == IS_DOUBLE
     || Z_TYPE_P(value) == IS_TRUE  || Z_TYPE_P(value) == IS_FALSE
     || Z_TYPE_P(value) == IS_NULL) {
        double dv;
        zend_long lv;
        zval_to_numeric(value, &dv, &lv);

        zend_long idx_arr[NUMPHP_MAX_NDIM] = {0};
        for (zend_long n = 0; n < sub_size; n++) {
            zend_long off = target_offset;
            for (int i = 0; i < sub_ndim; i++) off += idx_arr[i] * sub_strides[i];
            write_scalar_at((char *)a->buffer->data + off, a->dtype, dv, lv);
            for (int i = sub_ndim - 1; i >= 0; i--) {
                if (++idx_arr[i] < sub_shape[i]) break;
                idx_arr[i] = 0;
            }
        }
        return;
    }

    /* NDArray RHS — shape must match sub-block */
    if (Z_TYPE_P(value) == IS_OBJECT
     && instanceof_function(Z_OBJCE_P(value), numphp_ndarray_ce)) {
        numphp_ndarray *src = Z_NDARRAY_P(value);
        if (src->ndim != sub_ndim) {
            zend_throw_exception_ex(numphp_shape_exception_ce, 0,
                "Shape mismatch in assignment: expected %dD, got %dD", sub_ndim, src->ndim);
            return;
        }
        for (int i = 0; i < sub_ndim; i++) {
            if (src->shape[i] != sub_shape[i]) {
                zend_throw_exception(numphp_shape_exception_ce,
                    "Shape mismatch in assignment", 0);
                return;
            }
        }
        zend_long idx_arr[NUMPHP_MAX_NDIM] = {0};
        for (zend_long n = 0; n < sub_size; n++) {
            zend_long dst_off = target_offset, src_off = src->offset;
            for (int i = 0; i < sub_ndim; i++) {
                dst_off += idx_arr[i] * sub_strides[i];
                src_off += idx_arr[i] * src->strides[i];
            }
            double sv_d = 0.0;
            zend_long sv_l = 0;
            read_scalar_at((char *)src->buffer->data + src_off, src->dtype, &sv_d, &sv_l);
            write_scalar_at((char *)a->buffer->data + dst_off, a->dtype, sv_d, sv_l);
            for (int i = sub_ndim - 1; i >= 0; i--) {
                if (++idx_arr[i] < sub_shape[i]) break;
                idx_arr[i] = 0;
            }
        }
        return;
    }

    zend_throw_exception(numphp_ndarray_exception_ce,
        "Assignment value must be a scalar or NDArray "
        "(PHP-array RHS deferred to a later sprint)", 0);
}

static int numphp_offset_has(zend_object *zo, zval *offset, int check_empty)
{
    (void)check_empty;
    numphp_ndarray *a = numphp_obj_from_zo(zo)->array;
    if (Z_TYPE_P(offset) != IS_LONG) return 0;
    if (a->ndim == 0) return 0;
    zend_long idx = Z_LVAL_P(offset);
    if (idx < 0) idx += a->shape[0];
    return (idx >= 0 && idx < a->shape[0]) ? 1 : 0;
}

static void numphp_offset_unset(zend_object *zo, zval *offset)
{
    (void)zo;
    (void)offset;
    zend_throw_exception(numphp_ndarray_exception_ce,
        "NDArray does not support unset; use slicing or recreate the array", 0);
}

static zend_result numphp_count_elements(zend_object *zo, zend_long *count)
{
    numphp_ndarray *a = numphp_obj_from_zo(zo)->array;
    *count = a->size;
    return SUCCESS;
}

/* ===== ArrayAccess / Countable PHP method stubs (delegate to handlers) ===== */

PHP_METHOD(NDArray, offsetGet)
{
    zval *offset;
    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_ZVAL(offset)
    ZEND_PARSE_PARAMETERS_END();
    numphp_offset_get(Z_OBJ_P(ZEND_THIS), offset, BP_VAR_R, return_value);
}

PHP_METHOD(NDArray, offsetSet)
{
    zval *offset, *value;
    ZEND_PARSE_PARAMETERS_START(2, 2)
        Z_PARAM_ZVAL(offset)
        Z_PARAM_ZVAL(value)
    ZEND_PARSE_PARAMETERS_END();
    (void)return_value;
    numphp_offset_set(Z_OBJ_P(ZEND_THIS), offset, value);
}

PHP_METHOD(NDArray, offsetExists)
{
    zval *offset;
    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_ZVAL(offset)
    ZEND_PARSE_PARAMETERS_END();
    RETURN_BOOL(numphp_offset_has(Z_OBJ_P(ZEND_THIS), offset, 0));
}

PHP_METHOD(NDArray, offsetUnset)
{
    zval *offset;
    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_ZVAL(offset)
    ZEND_PARSE_PARAMETERS_END();
    (void)return_value;
    numphp_offset_unset(Z_OBJ_P(ZEND_THIS), offset);
}

PHP_METHOD(NDArray, count)
{
    ZEND_PARSE_PARAMETERS_NONE();
    numphp_ndarray *a = Z_NDARRAY_P(ZEND_THIS);
    RETURN_LONG(a->size);
}

/* ===== element-wise binary ops ===== */

typedef enum { OP_ADD, OP_SUB, OP_MUL, OP_DIV } numphp_binop;

static numphp_ndarray *scalar_to_0d_ndarray(zval *v)
{
    numphp_dtype dt = (Z_TYPE_P(v) == IS_DOUBLE) ? NUMPHP_FLOAT64 : NUMPHP_INT64;
    numphp_ndarray *a = numphp_ndarray_alloc_owner(dt, 0, NULL);
    double dv = 0.0;
    zend_long lv = 0;
    zval_to_numeric(v, &dv, &lv);
    write_scalar_at((char *)a->buffer->data + a->offset, dt, dv, lv);
    return a;
}

static int do_binary_op_core(zend_uchar opcode, zval *result, zval *op1, zval *op2)
{
    numphp_binop op;
    switch (opcode) {
        case ZEND_ADD: op = OP_ADD; break;
        case ZEND_SUB: op = OP_SUB; break;
        case ZEND_MUL: op = OP_MUL; break;
        case ZEND_DIV: op = OP_DIV; break;
        default: return FAILURE;
    }

    numphp_ndarray *a, *b;
    int free_a = 0, free_b = 0;

    if (Z_TYPE_P(op1) == IS_OBJECT
     && instanceof_function(Z_OBJCE_P(op1), numphp_ndarray_ce)) {
        a = Z_NDARRAY_P(op1);
    } else {
        a = scalar_to_0d_ndarray(op1);
        free_a = 1;
    }
    if (Z_TYPE_P(op2) == IS_OBJECT
     && instanceof_function(Z_OBJCE_P(op2), numphp_ndarray_ce)) {
        b = Z_NDARRAY_P(op2);
    } else {
        b = scalar_to_0d_ndarray(op2);
        free_b = 1;
    }

    int out_ndim;
    zend_long out_shape[NUMPHP_MAX_NDIM];
    numphp_ndarray *ops[2] = { a, b };
    if (!numphp_broadcast_shape(2, ops, &out_ndim, out_shape)) {
        if (free_a) numphp_ndarray_free(a);
        if (free_b) numphp_ndarray_free(b);
        return FAILURE;
    }

    numphp_dtype out_dt = numphp_promote_dtype(a->dtype, b->dtype);
    numphp_ndarray *out = numphp_ndarray_alloc_owner(out_dt, out_ndim, out_shape);

    numphp_nditer it;
    if (!numphp_nditer_init(&it, 2, ops, out)) {
        numphp_ndarray_free(out);
        if (free_a) numphp_ndarray_free(a);
        if (free_b) numphp_ndarray_free(b);
        return FAILURE;
    }

    int ok = 1;
    if (it.size > 0) do {
        switch (out_dt) {
            case NUMPHP_FLOAT64: {
                double av = numphp_read_f64(it.ptr[0], a->dtype);
                double bv = numphp_read_f64(it.ptr[1], b->dtype);
                double rv = 0.0;
                switch (op) {
                    case OP_ADD: rv = av + bv; break;
                    case OP_SUB: rv = av - bv; break;
                    case OP_MUL: rv = av * bv; break;
                    case OP_DIV: rv = av / bv; break;
                }
                *(double *)it.ptr[2] = rv;
                break;
            }
            case NUMPHP_FLOAT32: {
                float av = numphp_read_f32(it.ptr[0], a->dtype);
                float bv = numphp_read_f32(it.ptr[1], b->dtype);
                float rv = 0.0f;
                switch (op) {
                    case OP_ADD: rv = av + bv; break;
                    case OP_SUB: rv = av - bv; break;
                    case OP_MUL: rv = av * bv; break;
                    case OP_DIV: rv = av / bv; break;
                }
                *(float *)it.ptr[2] = rv;
                break;
            }
            case NUMPHP_INT64: {
                int64_t av = numphp_read_i64(it.ptr[0], a->dtype);
                int64_t bv = numphp_read_i64(it.ptr[1], b->dtype);
                if (op == OP_DIV && bv == 0) {
                    zend_throw_exception(zend_ce_division_by_zero_error, "Division by zero", 0);
                    ok = 0;
                    break;
                }
                int64_t rv = 0;
                switch (op) {
                    case OP_ADD: rv = av + bv; break;
                    case OP_SUB: rv = av - bv; break;
                    case OP_MUL: rv = av * bv; break;
                    case OP_DIV: rv = av / bv; break;
                }
                *(int64_t *)it.ptr[2] = rv;
                break;
            }
            case NUMPHP_INT32: {
                int32_t av = (int32_t)numphp_read_i64(it.ptr[0], a->dtype);
                int32_t bv = (int32_t)numphp_read_i64(it.ptr[1], b->dtype);
                if (op == OP_DIV && bv == 0) {
                    zend_throw_exception(zend_ce_division_by_zero_error, "Division by zero", 0);
                    ok = 0;
                    break;
                }
                int32_t rv = 0;
                switch (op) {
                    case OP_ADD: rv = av + bv; break;
                    case OP_SUB: rv = av - bv; break;
                    case OP_MUL: rv = av * bv; break;
                    case OP_DIV: rv = av / bv; break;
                }
                *(int32_t *)it.ptr[2] = rv;
                break;
            }
        }
        if (!ok) break;
        numphp_nditer_next(&it);
    } while (!it.done);

    if (free_a) numphp_ndarray_free(a);
    if (free_b) numphp_ndarray_free(b);

    if (!ok) {
        numphp_ndarray_free(out);
        return FAILURE;
    }

    numphp_zval_wrap_ndarray(result, out);
    return SUCCESS;
}

static zend_result numphp_do_operation(zend_uchar opcode, zval *result, zval *op1, zval *op2)
{
    /* Only handle the four arithmetic ops; fall through (FAILURE) for everything else
     * so PHP can dispatch the engine default and emit its own error. */
    if (opcode != ZEND_ADD && opcode != ZEND_SUB
     && opcode != ZEND_MUL && opcode != ZEND_DIV) {
        return FAILURE;
    }
    return (zend_result)do_binary_op_core(opcode, result, op1, op2);
}

PHP_METHOD(NDArray, add)
{
    zval *a, *b;
    ZEND_PARSE_PARAMETERS_START(2, 2)
        Z_PARAM_ZVAL(a)
        Z_PARAM_ZVAL(b)
    ZEND_PARSE_PARAMETERS_END();
    if (do_binary_op_core(ZEND_ADD, return_value, a, b) == FAILURE) RETURN_THROWS();
}

PHP_METHOD(NDArray, subtract)
{
    zval *a, *b;
    ZEND_PARSE_PARAMETERS_START(2, 2)
        Z_PARAM_ZVAL(a)
        Z_PARAM_ZVAL(b)
    ZEND_PARSE_PARAMETERS_END();
    if (do_binary_op_core(ZEND_SUB, return_value, a, b) == FAILURE) RETURN_THROWS();
}

PHP_METHOD(NDArray, multiply)
{
    zval *a, *b;
    ZEND_PARSE_PARAMETERS_START(2, 2)
        Z_PARAM_ZVAL(a)
        Z_PARAM_ZVAL(b)
    ZEND_PARSE_PARAMETERS_END();
    if (do_binary_op_core(ZEND_MUL, return_value, a, b) == FAILURE) RETURN_THROWS();
}

PHP_METHOD(NDArray, divide)
{
    zval *a, *b;
    ZEND_PARSE_PARAMETERS_START(2, 2)
        Z_PARAM_ZVAL(a)
        Z_PARAM_ZVAL(b)
    ZEND_PARSE_PARAMETERS_END();
    if (do_binary_op_core(ZEND_DIV, return_value, a, b) == FAILURE) RETURN_THROWS();
}

/* ===== shape manipulation ===== */

/* Element-by-element copy from src (any layout) into a freshly allocated C-contiguous owner */
numphp_ndarray *numphp_materialize_contiguous(numphp_ndarray *src)
{
    numphp_ndarray *out = numphp_ndarray_alloc_owner(src->dtype, src->ndim, src->shape);
    if (src->size == 0) return out;

    char *src_base = (char *)src->buffer->data + src->offset;
    char *dst = (char *)out->buffer->data;
    zend_long itemsize = src->itemsize;

    if (src->ndim == 0) {
        memcpy(dst, src_base, (size_t)itemsize);
        return out;
    }

    zend_long idx[NUMPHP_MAX_NDIM] = {0};
    for (zend_long n = 0; n < src->size; n++) {
        zend_long src_off = 0;
        for (int j = 0; j < src->ndim; j++) src_off += idx[j] * src->strides[j];
        memcpy(dst + n * itemsize, src_base + src_off, (size_t)itemsize);
        for (int j = src->ndim - 1; j >= 0; j--) {
            if (++idx[j] < src->shape[j]) break;
            idx[j] = 0;
        }
    }
    return out;
}

PHP_METHOD(NDArray, reshape)
{
    zval *shape_arr;
    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_ARRAY(shape_arr)
    ZEND_PARSE_PARAMETERS_END();

    numphp_ndarray *a = Z_NDARRAY_P(ZEND_THIS);

    zend_long shape[NUMPHP_MAX_NDIM];
    int ndim = 0;
    int infer_idx = -1;
    zend_long known_product = 1;
    HashTable *ht = Z_ARRVAL_P(shape_arr);
    int i = 0;
    zval *v;
    ZEND_HASH_FOREACH_VAL(ht, v) {
        ZVAL_DEREF(v);
        if (Z_TYPE_P(v) != IS_LONG) {
            zend_throw_exception(numphp_shape_exception_ce, "reshape: shape entries must be integers", 0);
            RETURN_THROWS();
        }
        zend_long d = Z_LVAL_P(v);
        if (d == -1) {
            if (infer_idx >= 0) {
                zend_throw_exception(numphp_shape_exception_ce, "reshape: only one -1 placeholder allowed", 0);
                RETURN_THROWS();
            }
            infer_idx = i;
            shape[i++] = -1;
        } else if (d < 0) {
            zend_throw_exception(numphp_shape_exception_ce, "reshape: shape entries must be non-negative or -1", 0);
            RETURN_THROWS();
        } else {
            shape[i++] = d;
            known_product *= d;
        }
    } ZEND_HASH_FOREACH_END();
    ndim = i;

    if (infer_idx >= 0) {
        if (known_product == 0) {
            zend_throw_exception(numphp_shape_exception_ce, "reshape: cannot infer dimension when other dims are zero", 0);
            RETURN_THROWS();
        }
        if (a->size % known_product != 0) {
            zend_throw_exception_ex(numphp_shape_exception_ce, 0,
                "reshape: total size %lld not divisible by known dims product %lld",
                (long long)a->size, (long long)known_product);
            RETURN_THROWS();
        }
        shape[infer_idx] = a->size / known_product;
    }

    zend_long total = 1;
    for (int j = 0; j < ndim; j++) total *= shape[j];
    if (total != a->size) {
        zend_throw_exception_ex(numphp_shape_exception_ce, 0,
            "reshape: total size mismatch (%lld vs %lld)",
            (long long)total, (long long)a->size);
        RETURN_THROWS();
    }

    if (a->flags & NUMPHP_C_CONTIGUOUS) {
        zend_long new_strides[NUMPHP_MAX_NDIM];
        if (ndim > 0) {
            new_strides[ndim - 1] = a->itemsize;
            for (int j = ndim - 2; j >= 0; j--) {
                new_strides[j] = new_strides[j + 1] * shape[j + 1];
            }
        }
        numphp_ndarray *view = numphp_ndarray_alloc_view(a, ndim, shape, new_strides, a->offset);
        numphp_zval_wrap_ndarray(return_value, view);
        return;
    }

    /* Non-contiguous source: copy to C-contiguous, then build the reshaped owner */
    numphp_ndarray *contig = numphp_materialize_contiguous(a);
    /* Reshape in place: same buffer, new shape/strides */
    if (contig->shape) efree(contig->shape);
    if (contig->strides) efree(contig->strides);
    contig->ndim = ndim;
    contig->shape = (ndim > 0) ? emalloc(sizeof(zend_long) * ndim) : NULL;
    contig->strides = (ndim > 0) ? emalloc(sizeof(zend_long) * ndim) : NULL;
    if (ndim > 0) {
        contig->strides[ndim - 1] = contig->itemsize;
        for (int j = ndim - 2; j >= 0; j--) {
            contig->strides[j] = contig->strides[j + 1] * shape[j + 1];
        }
    }
    for (int j = 0; j < ndim; j++) contig->shape[j] = shape[j];
    numphp_ndarray_recompute_contiguity(contig);
    numphp_zval_wrap_ndarray(return_value, contig);
}

PHP_METHOD(NDArray, transpose)
{
    zval *axes = NULL;
    ZEND_PARSE_PARAMETERS_START(0, 1)
        Z_PARAM_OPTIONAL
        Z_PARAM_ARRAY_OR_NULL(axes)
    ZEND_PARSE_PARAMETERS_END();

    numphp_ndarray *a = Z_NDARRAY_P(ZEND_THIS);
    int ndim = a->ndim;
    int perm[NUMPHP_MAX_NDIM];

    if (axes) {
        HashTable *ht = Z_ARRVAL_P(axes);
        if ((int)zend_hash_num_elements(ht) != ndim) {
            zend_throw_exception(numphp_shape_exception_ce, "transpose: axes must have ndim entries", 0);
            RETURN_THROWS();
        }
        int seen[NUMPHP_MAX_NDIM] = {0};
        int i = 0;
        zval *v;
        ZEND_HASH_FOREACH_VAL(ht, v) {
            ZVAL_DEREF(v);
            if (Z_TYPE_P(v) != IS_LONG) {
                zend_throw_exception(numphp_shape_exception_ce, "transpose: axis entries must be integers", 0);
                RETURN_THROWS();
            }
            zend_long ax = Z_LVAL_P(v);
            if (ax < 0) ax += ndim;
            if (ax < 0 || ax >= ndim || seen[ax]) {
                zend_throw_exception(numphp_shape_exception_ce, "transpose: invalid or duplicate axis", 0);
                RETURN_THROWS();
            }
            seen[ax] = 1;
            perm[i++] = (int)ax;
        } ZEND_HASH_FOREACH_END();
    } else {
        for (int i = 0; i < ndim; i++) perm[i] = ndim - 1 - i;
    }

    zend_long new_shape[NUMPHP_MAX_NDIM];
    zend_long new_strides[NUMPHP_MAX_NDIM];
    for (int i = 0; i < ndim; i++) {
        new_shape[i]   = a->shape[perm[i]];
        new_strides[i] = a->strides[perm[i]];
    }

    numphp_ndarray *view = numphp_ndarray_alloc_view(a, ndim, new_shape, new_strides, a->offset);
    numphp_zval_wrap_ndarray(return_value, view);
}

PHP_METHOD(NDArray, flatten)
{
    ZEND_PARSE_PARAMETERS_NONE();
    numphp_ndarray *a = Z_NDARRAY_P(ZEND_THIS);

    zend_long shape[1] = { a->size };
    numphp_ndarray *out = numphp_ndarray_alloc_owner(a->dtype, 1, shape);

    if (a->size == 0) {
        numphp_zval_wrap_ndarray(return_value, out);
        return;
    }

    char *src_base = (char *)a->buffer->data + a->offset;
    char *dst = (char *)out->buffer->data;
    zend_long itemsize = a->itemsize;

    if (a->ndim == 0) {
        memcpy(dst, src_base, (size_t)itemsize);
    } else {
        zend_long idx[NUMPHP_MAX_NDIM] = {0};
        for (zend_long n = 0; n < a->size; n++) {
            zend_long src_off = 0;
            for (int j = 0; j < a->ndim; j++) src_off += idx[j] * a->strides[j];
            memcpy(dst + n * itemsize, src_base + src_off, (size_t)itemsize);
            for (int j = a->ndim - 1; j >= 0; j--) {
                if (++idx[j] < a->shape[j]) break;
                idx[j] = 0;
            }
        }
    }

    numphp_zval_wrap_ndarray(return_value, out);
}

PHP_METHOD(NDArray, squeeze)
{
    zend_long axis = 0;
    zend_bool axis_is_null = 1;
    ZEND_PARSE_PARAMETERS_START(0, 1)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG_OR_NULL(axis, axis_is_null)
    ZEND_PARSE_PARAMETERS_END();

    numphp_ndarray *a = Z_NDARRAY_P(ZEND_THIS);
    int ndim = a->ndim;
    int new_ndim = 0;
    zend_long new_shape[NUMPHP_MAX_NDIM];
    zend_long new_strides[NUMPHP_MAX_NDIM];

    if (axis_is_null) {
        for (int i = 0; i < ndim; i++) {
            if (a->shape[i] != 1) {
                new_shape[new_ndim] = a->shape[i];
                new_strides[new_ndim] = a->strides[i];
                new_ndim++;
            }
        }
    } else {
        if (axis < 0) axis += ndim;
        if (axis < 0 || axis >= ndim) {
            zend_throw_exception(numphp_index_exception_ce, "squeeze: axis out of range", 0);
            RETURN_THROWS();
        }
        if (a->shape[axis] != 1) {
            zend_throw_exception(numphp_shape_exception_ce, "squeeze: cannot squeeze axis with size != 1", 0);
            RETURN_THROWS();
        }
        for (int i = 0; i < ndim; i++) {
            if (i == axis) continue;
            new_shape[new_ndim] = a->shape[i];
            new_strides[new_ndim] = a->strides[i];
            new_ndim++;
        }
    }

    numphp_ndarray *view = numphp_ndarray_alloc_view(a, new_ndim, new_shape, new_strides, a->offset);
    numphp_zval_wrap_ndarray(return_value, view);
}

PHP_METHOD(NDArray, expandDims)
{
    zend_long axis;
    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_LONG(axis)
    ZEND_PARSE_PARAMETERS_END();

    numphp_ndarray *a = Z_NDARRAY_P(ZEND_THIS);
    int ndim = a->ndim;
    if (axis < 0) axis += ndim + 1;
    if (axis < 0 || axis > ndim) {
        zend_throw_exception(numphp_index_exception_ce, "expandDims: axis out of range", 0);
        RETURN_THROWS();
    }

    zend_long new_shape[NUMPHP_MAX_NDIM];
    zend_long new_strides[NUMPHP_MAX_NDIM];
    int j = 0;
    for (int i = 0; i <= ndim; i++) {
        if (i == axis) {
            new_shape[i] = 1;
            new_strides[i] = 0;
        } else {
            new_shape[i] = a->shape[j];
            new_strides[i] = a->strides[j];
            j++;
        }
    }

    numphp_ndarray *view = numphp_ndarray_alloc_view(a, ndim + 1, new_shape, new_strides, a->offset);
    numphp_zval_wrap_ndarray(return_value, view);
}

#define NUMPHP_CONCAT_MAX 64

static int collect_ndarray_inputs(zval *arrays, numphp_ndarray **out_arrs, int *out_n, const char *who)
{
    HashTable *ht = Z_ARRVAL_P(arrays);
    int n = (int)zend_hash_num_elements(ht);
    if (n == 0) {
        zend_throw_exception_ex(numphp_shape_exception_ce, 0, "%s: no arrays provided", who);
        return 0;
    }
    if (n > NUMPHP_CONCAT_MAX) {
        zend_throw_exception_ex(numphp_shape_exception_ce, 0,
            "%s: too many arrays (max %d)", who, NUMPHP_CONCAT_MAX);
        return 0;
    }
    int i = 0;
    zval *v;
    ZEND_HASH_FOREACH_VAL(ht, v) {
        ZVAL_DEREF(v);
        if (Z_TYPE_P(v) != IS_OBJECT
         || !instanceof_function(Z_OBJCE_P(v), numphp_ndarray_ce)) {
            zend_throw_exception_ex(numphp_ndarray_exception_ce, 0,
                "%s: all elements must be NDArrays", who);
            return 0;
        }
        out_arrs[i++] = Z_NDARRAY_P(v);
    } ZEND_HASH_FOREACH_END();
    *out_n = n;
    return 1;
}

PHP_METHOD(NDArray, concatenate)
{
    zval *arrays;
    zend_long axis = 0;
    ZEND_PARSE_PARAMETERS_START(1, 2)
        Z_PARAM_ARRAY(arrays)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(axis)
    ZEND_PARSE_PARAMETERS_END();

    numphp_ndarray *arrs[NUMPHP_CONCAT_MAX];
    int n;
    if (!collect_ndarray_inputs(arrays, arrs, &n, "concatenate")) RETURN_THROWS();

    int ndim = arrs[0]->ndim;
    if (ndim == 0) {
        zend_throw_exception(numphp_shape_exception_ce, "concatenate: 0-D arrays cannot be concatenated", 0);
        RETURN_THROWS();
    }
    if (axis < 0) axis += ndim;
    if (axis < 0 || axis >= ndim) {
        zend_throw_exception(numphp_index_exception_ce, "concatenate: axis out of range", 0);
        RETURN_THROWS();
    }

    zend_long axis_sum = 0;
    numphp_dtype out_dt = arrs[0]->dtype;
    for (int k = 0; k < n; k++) {
        if (arrs[k]->ndim != ndim) {
            zend_throw_exception(numphp_shape_exception_ce, "concatenate: ndim mismatch", 0);
            RETURN_THROWS();
        }
        for (int j = 0; j < ndim; j++) {
            if (j == axis) continue;
            if (arrs[k]->shape[j] != arrs[0]->shape[j]) {
                zend_throw_exception(numphp_shape_exception_ce, "concatenate: shape mismatch off concat axis", 0);
                RETURN_THROWS();
            }
        }
        axis_sum += arrs[k]->shape[axis];
        if (k > 0) out_dt = numphp_promote_dtype(out_dt, arrs[k]->dtype);
    }

    zend_long out_shape[NUMPHP_MAX_NDIM];
    for (int j = 0; j < ndim; j++) out_shape[j] = arrs[0]->shape[j];
    out_shape[axis] = axis_sum;

    numphp_ndarray *out = numphp_ndarray_alloc_owner(out_dt, ndim, out_shape);

    zend_long out_axis_pos = 0;
    for (int k = 0; k < n; k++) {
        numphp_ndarray *src = arrs[k];
        zend_long idx[NUMPHP_MAX_NDIM] = {0};
        for (zend_long m = 0; m < src->size; m++) {
            zend_long src_off = src->offset;
            zend_long out_off = 0;
            for (int j = 0; j < ndim; j++) {
                src_off += idx[j] * src->strides[j];
                zend_long oi = (j == axis) ? out_axis_pos + idx[j] : idx[j];
                out_off += oi * out->strides[j];
            }
            double sv_d = 0.0;
            zend_long sv_l = 0;
            read_scalar_at((char *)src->buffer->data + src_off, src->dtype, &sv_d, &sv_l);
            write_scalar_at((char *)out->buffer->data + out_off, out_dt, sv_d, sv_l);
            for (int j = ndim - 1; j >= 0; j--) {
                if (++idx[j] < src->shape[j]) break;
                idx[j] = 0;
            }
        }
        out_axis_pos += src->shape[axis];
    }

    numphp_zval_wrap_ndarray(return_value, out);
}

PHP_METHOD(NDArray, stack)
{
    zval *arrays;
    zend_long axis = 0;
    ZEND_PARSE_PARAMETERS_START(1, 2)
        Z_PARAM_ARRAY(arrays)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(axis)
    ZEND_PARSE_PARAMETERS_END();

    numphp_ndarray *arrs[NUMPHP_CONCAT_MAX];
    int n;
    if (!collect_ndarray_inputs(arrays, arrs, &n, "stack")) RETURN_THROWS();

    int src_ndim = arrs[0]->ndim;
    int out_ndim = src_ndim + 1;
    if (out_ndim > NUMPHP_MAX_NDIM) {
        zend_throw_exception(numphp_shape_exception_ce, "stack: result would exceed NUMPHP_MAX_NDIM", 0);
        RETURN_THROWS();
    }
    if (axis < 0) axis += out_ndim;
    if (axis < 0 || axis >= out_ndim) {
        zend_throw_exception(numphp_index_exception_ce, "stack: axis out of range", 0);
        RETURN_THROWS();
    }

    numphp_dtype out_dt = arrs[0]->dtype;
    for (int k = 0; k < n; k++) {
        if (arrs[k]->ndim != src_ndim) {
            zend_throw_exception(numphp_shape_exception_ce, "stack: shape mismatch (different ndim)", 0);
            RETURN_THROWS();
        }
        for (int j = 0; j < src_ndim; j++) {
            if (arrs[k]->shape[j] != arrs[0]->shape[j]) {
                zend_throw_exception(numphp_shape_exception_ce, "stack: shape mismatch", 0);
                RETURN_THROWS();
            }
        }
        if (k > 0) out_dt = numphp_promote_dtype(out_dt, arrs[k]->dtype);
    }

    zend_long out_shape[NUMPHP_MAX_NDIM];
    {
        int sj = 0;
        for (int oi = 0; oi < out_ndim; oi++) {
            out_shape[oi] = (oi == axis) ? n : arrs[0]->shape[sj++];
        }
    }

    numphp_ndarray *out = numphp_ndarray_alloc_owner(out_dt, out_ndim, out_shape);

    for (int k = 0; k < n; k++) {
        numphp_ndarray *src = arrs[k];
        zend_long idx[NUMPHP_MAX_NDIM] = {0};
        zend_long src_size = src->size;
        if (src_size == 0 && src_ndim > 0) continue;
        for (zend_long m = 0; m < (src_size > 0 ? src_size : 1); m++) {
            zend_long src_off = src->offset;
            for (int j = 0; j < src_ndim; j++) src_off += idx[j] * src->strides[j];

            zend_long out_off = 0;
            int sj = 0;
            for (int oi = 0; oi < out_ndim; oi++) {
                zend_long ov = (oi == axis) ? k : idx[sj++];
                out_off += ov * out->strides[oi];
            }

            double sv_d = 0.0;
            zend_long sv_l = 0;
            read_scalar_at((char *)src->buffer->data + src_off, src->dtype, &sv_d, &sv_l);
            write_scalar_at((char *)out->buffer->data + out_off, out_dt, sv_d, sv_l);

            if (src_ndim == 0) break;
            for (int j = src_ndim - 1; j >= 0; j--) {
                if (++idx[j] < src->shape[j]) break;
                idx[j] = 0;
            }
        }
    }

    numphp_zval_wrap_ndarray(return_value, out);
}

/* ===== BLAS integration: dot, matmul, inner, outer ===== */

/* Returns src if already C-contiguous and of target_dt; otherwise a fresh contiguous
 * owner with values cast to target_dt. Sets *out_owned = 1 when caller must free. */
static numphp_ndarray *ensure_contig_dtype(numphp_ndarray *src, numphp_dtype target_dt, int *out_owned)
{
    if (src->dtype == target_dt && (src->flags & NUMPHP_C_CONTIGUOUS)) {
        *out_owned = 0;
        return src;
    }

    numphp_ndarray *out = numphp_ndarray_alloc_owner(target_dt, src->ndim, src->shape);
    *out_owned = 1;

    if (src->size == 0) return out;

    char *src_base = (char *)src->buffer->data + src->offset;
    char *dst_base = (char *)out->buffer->data;

    if (src->ndim == 0) {
        double sv_d = 0.0;
        zend_long sv_l = 0;
        read_scalar_at(src_base, src->dtype, &sv_d, &sv_l);
        write_scalar_at(dst_base, target_dt, sv_d, sv_l);
        return out;
    }

    zend_long idx[NUMPHP_MAX_NDIM] = {0};
    zend_long itemsize = out->itemsize;
    for (zend_long n = 0; n < src->size; n++) {
        zend_long src_off = 0;
        for (int j = 0; j < src->ndim; j++) src_off += idx[j] * src->strides[j];
        double sv_d = 0.0;
        zend_long sv_l = 0;
        read_scalar_at(src_base + src_off, src->dtype, &sv_d, &sv_l);
        write_scalar_at(dst_base + n * itemsize, target_dt, sv_d, sv_l);
        for (int j = src->ndim - 1; j >= 0; j--) {
            if (++idx[j] < src->shape[j]) break;
            idx[j] = 0;
        }
    }
    return out;
}

static numphp_dtype blas_target_dtype(numphp_ndarray *a, numphp_ndarray *b)
{
    /* If both inputs are float32, run the s-path; everything else promotes to float64. */
    return (a->dtype == NUMPHP_FLOAT32 && b->dtype == NUMPHP_FLOAT32)
         ? NUMPHP_FLOAT32 : NUMPHP_FLOAT64;
}

PHP_METHOD(NDArray, dot)
{
    zval *a_z, *b_z;
    ZEND_PARSE_PARAMETERS_START(2, 2)
        Z_PARAM_OBJECT_OF_CLASS(a_z, numphp_ndarray_ce)
        Z_PARAM_OBJECT_OF_CLASS(b_z, numphp_ndarray_ce)
    ZEND_PARSE_PARAMETERS_END();

    numphp_ndarray *a = Z_NDARRAY_P(a_z);
    numphp_ndarray *b = Z_NDARRAY_P(b_z);

    if (a->ndim != 1 || b->ndim != 1) {
        zend_throw_exception(numphp_shape_exception_ce, "dot: both inputs must be 1-D", 0);
        RETURN_THROWS();
    }
    if (a->shape[0] != b->shape[0]) {
        zend_throw_exception_ex(numphp_shape_exception_ce, 0,
            "dot: shape mismatch (%lld vs %lld)",
            (long long)a->shape[0], (long long)b->shape[0]);
        RETURN_THROWS();
    }

    numphp_dtype dt = blas_target_dtype(a, b);
    int own_a, own_b;
    numphp_ndarray *ca = ensure_contig_dtype(a, dt, &own_a);
    numphp_ndarray *cb = ensure_contig_dtype(b, dt, &own_b);

    int n = (int)ca->shape[0];
    double r;
    if (dt == NUMPHP_FLOAT64) {
        r = cblas_ddot(n, (const double *)ca->buffer->data, 1,
                          (const double *)cb->buffer->data, 1);
    } else {
        r = (double)cblas_sdot(n, (const float *)ca->buffer->data, 1,
                                  (const float *)cb->buffer->data, 1);
    }

    if (own_a) numphp_ndarray_free(ca);
    if (own_b) numphp_ndarray_free(cb);

    RETURN_DOUBLE(r);
}

PHP_METHOD(NDArray, matmul)
{
    zval *a_z, *b_z;
    ZEND_PARSE_PARAMETERS_START(2, 2)
        Z_PARAM_OBJECT_OF_CLASS(a_z, numphp_ndarray_ce)
        Z_PARAM_OBJECT_OF_CLASS(b_z, numphp_ndarray_ce)
    ZEND_PARSE_PARAMETERS_END();

    numphp_ndarray *a = Z_NDARRAY_P(a_z);
    numphp_ndarray *b = Z_NDARRAY_P(b_z);

    if (a->ndim != 2 || b->ndim != 2) {
        zend_throw_exception(numphp_shape_exception_ce,
            "matmul: both inputs must be 2-D in v1 (batched matmul deferred)", 0);
        RETURN_THROWS();
    }
    if (a->shape[1] != b->shape[0]) {
        zend_throw_exception_ex(numphp_shape_exception_ce, 0,
            "matmul: shape mismatch (A cols %lld != B rows %lld)",
            (long long)a->shape[1], (long long)b->shape[0]);
        RETURN_THROWS();
    }

    numphp_dtype dt = blas_target_dtype(a, b);
    int own_a, own_b;
    numphp_ndarray *ca = ensure_contig_dtype(a, dt, &own_a);
    numphp_ndarray *cb = ensure_contig_dtype(b, dt, &own_b);

    int M = (int)ca->shape[0];
    int K = (int)ca->shape[1];
    int N = (int)cb->shape[1];

    zend_long out_shape[2] = { M, N };
    numphp_ndarray *out = numphp_ndarray_alloc_owner(dt, 2, out_shape);

    if (M > 0 && N > 0) {
        if (K > 0) {
            if (dt == NUMPHP_FLOAT64) {
                cblas_dgemm(CblasRowMajor, CblasNoTrans, CblasNoTrans,
                            M, N, K,
                            1.0,
                            (const double *)ca->buffer->data, K,
                            (const double *)cb->buffer->data, N,
                            0.0,
                            (double *)out->buffer->data, N);
            } else {
                cblas_sgemm(CblasRowMajor, CblasNoTrans, CblasNoTrans,
                            M, N, K,
                            1.0f,
                            (const float *)ca->buffer->data, K,
                            (const float *)cb->buffer->data, N,
                            0.0f,
                            (float *)out->buffer->data, N);
            }
        } else {
            /* K = 0 → empty inner sum, result is all zeros */
            memset(out->buffer->data, 0, (size_t)out->buffer->nbytes);
        }
    }

    if (own_a) numphp_ndarray_free(ca);
    if (own_b) numphp_ndarray_free(cb);

    numphp_zval_wrap_ndarray(return_value, out);
}

PHP_METHOD(NDArray, inner)
{
    zval *a_z, *b_z;
    ZEND_PARSE_PARAMETERS_START(2, 2)
        Z_PARAM_OBJECT_OF_CLASS(a_z, numphp_ndarray_ce)
        Z_PARAM_OBJECT_OF_CLASS(b_z, numphp_ndarray_ce)
    ZEND_PARSE_PARAMETERS_END();

    numphp_ndarray *a = Z_NDARRAY_P(a_z);
    numphp_ndarray *b = Z_NDARRAY_P(b_z);

    if (a->ndim != 1 || b->ndim != 1) {
        zend_throw_exception(numphp_shape_exception_ce,
            "inner: only 1-D inputs supported in v1 (use matmul for higher-D)", 0);
        RETURN_THROWS();
    }
    if (a->shape[0] != b->shape[0]) {
        zend_throw_exception_ex(numphp_shape_exception_ce, 0,
            "inner: shape mismatch (%lld vs %lld)",
            (long long)a->shape[0], (long long)b->shape[0]);
        RETURN_THROWS();
    }

    numphp_dtype dt = blas_target_dtype(a, b);
    int own_a, own_b;
    numphp_ndarray *ca = ensure_contig_dtype(a, dt, &own_a);
    numphp_ndarray *cb = ensure_contig_dtype(b, dt, &own_b);

    int n = (int)ca->shape[0];
    double r;
    if (dt == NUMPHP_FLOAT64) {
        r = cblas_ddot(n, (const double *)ca->buffer->data, 1,
                          (const double *)cb->buffer->data, 1);
    } else {
        r = (double)cblas_sdot(n, (const float *)ca->buffer->data, 1,
                                  (const float *)cb->buffer->data, 1);
    }

    if (own_a) numphp_ndarray_free(ca);
    if (own_b) numphp_ndarray_free(cb);

    RETURN_DOUBLE(r);
}

PHP_METHOD(NDArray, outer)
{
    zval *a_z, *b_z;
    ZEND_PARSE_PARAMETERS_START(2, 2)
        Z_PARAM_OBJECT_OF_CLASS(a_z, numphp_ndarray_ce)
        Z_PARAM_OBJECT_OF_CLASS(b_z, numphp_ndarray_ce)
    ZEND_PARSE_PARAMETERS_END();

    numphp_ndarray *a = Z_NDARRAY_P(a_z);
    numphp_ndarray *b = Z_NDARRAY_P(b_z);

    if (a->ndim != 1 || b->ndim != 1) {
        zend_throw_exception(numphp_shape_exception_ce, "outer: both inputs must be 1-D", 0);
        RETURN_THROWS();
    }

    numphp_dtype dt = blas_target_dtype(a, b);
    int own_a, own_b;
    numphp_ndarray *ca = ensure_contig_dtype(a, dt, &own_a);
    numphp_ndarray *cb = ensure_contig_dtype(b, dt, &own_b);

    int M = (int)ca->shape[0];
    int N = (int)cb->shape[0];

    zend_long out_shape[2] = { M, N };
    numphp_ndarray *out = numphp_ndarray_alloc_owner(dt, 2, out_shape);
    if (out->buffer->nbytes > 0) {
        memset(out->buffer->data, 0, (size_t)out->buffer->nbytes);
    }

    if (M > 0 && N > 0) {
        if (dt == NUMPHP_FLOAT64) {
            cblas_dger(CblasRowMajor,
                       M, N,
                       1.0,
                       (const double *)ca->buffer->data, 1,
                       (const double *)cb->buffer->data, 1,
                       (double *)out->buffer->data, N);
        } else {
            cblas_sger(CblasRowMajor,
                       M, N,
                       1.0f,
                       (const float *)ca->buffer->data, 1,
                       (const float *)cb->buffer->data, 1,
                       (float *)out->buffer->data, N);
        }
    }

    if (own_a) numphp_ndarray_free(ca);
    if (own_b) numphp_ndarray_free(cb);

    numphp_zval_wrap_ndarray(return_value, out);
}

PHP_METHOD(NDArray, slice)
{
    zend_long start, stop;
    zend_long step = 1;

    ZEND_PARSE_PARAMETERS_START(2, 3)
        Z_PARAM_LONG(start)
        Z_PARAM_LONG(stop)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(step)
    ZEND_PARSE_PARAMETERS_END();

    numphp_ndarray *a = Z_NDARRAY_P(ZEND_THIS);
    if (a->ndim == 0) {
        zend_throw_exception(numphp_index_exception_ce, "Cannot slice a 0-D array", 0);
        RETURN_THROWS();
    }
    if (step <= 0) {
        zend_throw_exception(numphp_ndarray_exception_ce,
            "slice: step must be positive (negative step deferred)", 0);
        RETURN_THROWS();
    }

    zend_long n = a->shape[0];
    if (start < 0) start += n;
    if (stop  < 0) stop  += n;
    if (start < 0) start = 0;
    if (stop  > n) stop  = n;
    if (start > n) start = n;
    if (stop  < start) stop = start;

    zend_long count = (stop - start + step - 1) / step;
    if (count < 0) count = 0;

    zend_long new_shape[NUMPHP_MAX_NDIM];
    zend_long new_strides[NUMPHP_MAX_NDIM];
    new_shape[0] = count;
    new_strides[0] = a->strides[0] * step;
    for (int i = 1; i < a->ndim; i++) {
        new_shape[i]   = a->shape[i];
        new_strides[i] = a->strides[i];
    }
    zend_long new_offset = a->offset + start * a->strides[0];

    numphp_ndarray *view = numphp_ndarray_alloc_view(a, a->ndim, new_shape, new_strides, new_offset);
    numphp_zval_wrap_ndarray(return_value, view);
}

/* ===== reductions (sum / mean / min / max / std / var / argmin / argmax + nan-variants) ===== */

/* Build a scalar zval from a 0-D ndarray buffer. */
static void scalar_zval_from_buffer(zval *out, char *p, numphp_dtype dt)
{
    switch (dt) {
        case NUMPHP_FLOAT32: ZVAL_DOUBLE(out, *(float *)p);   break;
        case NUMPHP_FLOAT64: ZVAL_DOUBLE(out, *(double *)p);  break;
        case NUMPHP_INT32:   ZVAL_LONG(out, *(int32_t *)p);   break;
        case NUMPHP_INT64:   ZVAL_LONG(out, *(int64_t *)p);   break;
    }
}

static void do_reduce_method(INTERNAL_FUNCTION_PARAMETERS, numphp_reduce_op op, int skip_nan)
{
    zval *axis_zv = NULL;
    zend_bool keepdims = 0;
    zend_long ddof = 0;

    if (op == NUMPHP_REDUCE_VAR || op == NUMPHP_REDUCE_STD) {
        ZEND_PARSE_PARAMETERS_START(0, 3)
            Z_PARAM_OPTIONAL
            Z_PARAM_ZVAL(axis_zv)
            Z_PARAM_BOOL(keepdims)
            Z_PARAM_LONG(ddof)
        ZEND_PARSE_PARAMETERS_END();
    } else {
        ZEND_PARSE_PARAMETERS_START(0, 2)
            Z_PARAM_OPTIONAL
            Z_PARAM_ZVAL(axis_zv)
            Z_PARAM_BOOL(keepdims)
        ZEND_PARSE_PARAMETERS_END();
    }

    int has_axis = 0;
    int axis = 0;
    if (axis_zv && Z_TYPE_P(axis_zv) != IS_NULL) {
        if (Z_TYPE_P(axis_zv) != IS_LONG) {
            zend_throw_exception(numphp_shape_exception_ce, "axis must be int or null", 0);
            RETURN_THROWS();
        }
        has_axis = 1;
        axis = (int)Z_LVAL_P(axis_zv);
    }

    numphp_ndarray *a = Z_NDARRAY_P(ZEND_THIS);

    /* For integer dtype, nan-variants alias the regular form. */
    int eff_skip = skip_nan;
    if (eff_skip && (a->dtype == NUMPHP_INT32 || a->dtype == NUMPHP_INT64)) {
        eff_skip = 0;
    }

    numphp_ndarray *res = numphp_reduce(a, op, has_axis, axis, (int)keepdims, (int)ddof, eff_skip);
    if (!res) RETURN_THROWS();

    /* Unwrap 0-D result to scalar zval */
    if (!has_axis && !keepdims) {
        char *p = (char *)res->buffer->data;
        scalar_zval_from_buffer(return_value, p, res->dtype);
        numphp_ndarray_free(res);
        return;
    }
    numphp_zval_wrap_ndarray(return_value, res);
}

PHP_METHOD(NDArray, sum)        { do_reduce_method(INTERNAL_FUNCTION_PARAM_PASSTHRU, NUMPHP_REDUCE_SUM,    0); }
PHP_METHOD(NDArray, mean)       { do_reduce_method(INTERNAL_FUNCTION_PARAM_PASSTHRU, NUMPHP_REDUCE_MEAN,   0); }
PHP_METHOD(NDArray, min)        { do_reduce_method(INTERNAL_FUNCTION_PARAM_PASSTHRU, NUMPHP_REDUCE_MIN,    0); }
PHP_METHOD(NDArray, max)        { do_reduce_method(INTERNAL_FUNCTION_PARAM_PASSTHRU, NUMPHP_REDUCE_MAX,    0); }
PHP_METHOD(NDArray, var)        { do_reduce_method(INTERNAL_FUNCTION_PARAM_PASSTHRU, NUMPHP_REDUCE_VAR,    0); }
PHP_METHOD(NDArray, std)        { do_reduce_method(INTERNAL_FUNCTION_PARAM_PASSTHRU, NUMPHP_REDUCE_STD,    0); }
PHP_METHOD(NDArray, argmin)     { do_reduce_method(INTERNAL_FUNCTION_PARAM_PASSTHRU, NUMPHP_REDUCE_ARGMIN, 0); }
PHP_METHOD(NDArray, argmax)     { do_reduce_method(INTERNAL_FUNCTION_PARAM_PASSTHRU, NUMPHP_REDUCE_ARGMAX, 0); }

PHP_METHOD(NDArray, nansum)     { do_reduce_method(INTERNAL_FUNCTION_PARAM_PASSTHRU, NUMPHP_REDUCE_SUM,    1); }
PHP_METHOD(NDArray, nanmean)    { do_reduce_method(INTERNAL_FUNCTION_PARAM_PASSTHRU, NUMPHP_REDUCE_MEAN,   1); }
PHP_METHOD(NDArray, nanmin)     { do_reduce_method(INTERNAL_FUNCTION_PARAM_PASSTHRU, NUMPHP_REDUCE_MIN,    1); }
PHP_METHOD(NDArray, nanmax)     { do_reduce_method(INTERNAL_FUNCTION_PARAM_PASSTHRU, NUMPHP_REDUCE_MAX,    1); }
PHP_METHOD(NDArray, nanvar)     { do_reduce_method(INTERNAL_FUNCTION_PARAM_PASSTHRU, NUMPHP_REDUCE_VAR,    1); }
PHP_METHOD(NDArray, nanstd)     { do_reduce_method(INTERNAL_FUNCTION_PARAM_PASSTHRU, NUMPHP_REDUCE_STD,    1); }
PHP_METHOD(NDArray, nanargmin)  { do_reduce_method(INTERNAL_FUNCTION_PARAM_PASSTHRU, NUMPHP_REDUCE_ARGMIN, 1); }
PHP_METHOD(NDArray, nanargmax)  { do_reduce_method(INTERNAL_FUNCTION_PARAM_PASSTHRU, NUMPHP_REDUCE_ARGMAX, 1); }

/* ===== element-wise math ===== */

static void do_unary_method(INTERNAL_FUNCTION_PARAMETERS, numphp_math_op op)
{
    ZEND_PARSE_PARAMETERS_NONE();
    numphp_ndarray *a = Z_NDARRAY_P(ZEND_THIS);
    numphp_ndarray *out = numphp_apply_unary(a, op);
    if (!out) RETURN_THROWS();
    numphp_zval_wrap_ndarray(return_value, out);
}

PHP_METHOD(NDArray, sqrt_)  { do_unary_method(INTERNAL_FUNCTION_PARAM_PASSTHRU, NUMPHP_MATH_SQRT);  }
PHP_METHOD(NDArray, exp_)   { do_unary_method(INTERNAL_FUNCTION_PARAM_PASSTHRU, NUMPHP_MATH_EXP);   }
PHP_METHOD(NDArray, log_)   { do_unary_method(INTERNAL_FUNCTION_PARAM_PASSTHRU, NUMPHP_MATH_LOG);   }
PHP_METHOD(NDArray, log2_)  { do_unary_method(INTERNAL_FUNCTION_PARAM_PASSTHRU, NUMPHP_MATH_LOG2);  }
PHP_METHOD(NDArray, log10_) { do_unary_method(INTERNAL_FUNCTION_PARAM_PASSTHRU, NUMPHP_MATH_LOG10); }
PHP_METHOD(NDArray, abs_)   { do_unary_method(INTERNAL_FUNCTION_PARAM_PASSTHRU, NUMPHP_MATH_ABS);   }
PHP_METHOD(NDArray, floor_) { do_unary_method(INTERNAL_FUNCTION_PARAM_PASSTHRU, NUMPHP_MATH_FLOOR); }
PHP_METHOD(NDArray, ceil_)  { do_unary_method(INTERNAL_FUNCTION_PARAM_PASSTHRU, NUMPHP_MATH_CEIL);  }

PHP_METHOD(NDArray, clip)
{
    zval *minv = NULL, *maxv = NULL;
    ZEND_PARSE_PARAMETERS_START(2, 2)
        Z_PARAM_ZVAL_OR_NULL(minv)
        Z_PARAM_ZVAL_OR_NULL(maxv)
    ZEND_PARSE_PARAMETERS_END();

    int has_min = 0, has_max = 0;
    double dmin = 0.0, dmax = 0.0;
    if (minv && Z_TYPE_P(minv) != IS_NULL) {
        zend_long lv;
        zval_to_numeric(minv, &dmin, &lv);
        has_min = 1;
    }
    if (maxv && Z_TYPE_P(maxv) != IS_NULL) {
        zend_long lv;
        zval_to_numeric(maxv, &dmax, &lv);
        has_max = 1;
    }

    numphp_ndarray *a = Z_NDARRAY_P(ZEND_THIS);
    numphp_ndarray *out = numphp_apply_clip(a, has_min, dmin, has_max, dmax);
    if (!out) RETURN_THROWS();
    numphp_zval_wrap_ndarray(return_value, out);
}

PHP_METHOD(NDArray, round_)
{
    zend_long decimals = 0;
    ZEND_PARSE_PARAMETERS_START(0, 1)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(decimals)
    ZEND_PARSE_PARAMETERS_END();

    numphp_ndarray *a = Z_NDARRAY_P(ZEND_THIS);
    numphp_ndarray *out = numphp_apply_round(a, (int)decimals);
    if (!out) RETURN_THROWS();
    numphp_zval_wrap_ndarray(return_value, out);
}

PHP_METHOD(NDArray, power)
{
    zval *exp_zv;
    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_ZVAL(exp_zv)
    ZEND_PARSE_PARAMETERS_END();

    if (Z_TYPE_P(exp_zv) == IS_OBJECT
     && instanceof_function(Z_OBJCE_P(exp_zv), numphp_ndarray_ce)) {
        zend_throw_exception(numphp_ndarray_exception_ce,
            "power: array exponent not yet supported (use scalar)", 0);
        RETURN_THROWS();
    }
    double dv; zend_long lv;
    zval_to_numeric(exp_zv, &dv, &lv);

    numphp_ndarray *a = Z_NDARRAY_P(ZEND_THIS);
    numphp_ndarray *out = numphp_apply_power_scalar(a, dv);
    if (!out) RETURN_THROWS();
    numphp_zval_wrap_ndarray(return_value, out);
}

/* ===== sort / argsort ===== */

static void do_sort_method(INTERNAL_FUNCTION_PARAMETERS, int do_argsort)
{
    /* Disambiguation rules:
     *   - argument omitted (axis_zv == NULL at C layer) → axis = -1 (last)
     *   - explicit `null` from PHP                       → axis = flatten
     *   - explicit int                                   → that axis
     * The C-layer `axis_zv` pointer stays NULL when nothing was passed because
     * Z_PARAM_OPTIONAL leaves it untouched. */
    zval *axis_zv = NULL;
    ZEND_PARSE_PARAMETERS_START(0, 1)
        Z_PARAM_OPTIONAL
        Z_PARAM_ZVAL(axis_zv)
    ZEND_PARSE_PARAMETERS_END();

    int axis;
    if (!axis_zv) {
        axis = -1;
    } else if (Z_TYPE_P(axis_zv) == IS_NULL) {
        axis = NUMPHP_AXIS_FLATTEN;
    } else {
        if (Z_TYPE_P(axis_zv) != IS_LONG) {
            zend_throw_exception(numphp_shape_exception_ce, "axis must be int or null", 0);
            RETURN_THROWS();
        }
        axis = (int)Z_LVAL_P(axis_zv);
    }

    numphp_ndarray *a = Z_NDARRAY_P(ZEND_THIS);
    numphp_ndarray *out = do_argsort ? numphp_argsort(a, axis) : numphp_sort(a, axis);
    if (!out) RETURN_THROWS();
    numphp_zval_wrap_ndarray(return_value, out);
}

PHP_METHOD(NDArray, sort)    { do_sort_method(INTERNAL_FUNCTION_PARAM_PASSTHRU, 0); }
PHP_METHOD(NDArray, argsort) { do_sort_method(INTERNAL_FUNCTION_PARAM_PASSTHRU, 1); }

/* ===== arginfo ===== */

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(arginfo_zeros, 0, 1, NDArray, 0)
    ZEND_ARG_TYPE_INFO(0, shape, IS_ARRAY, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, dtype, IS_STRING, 0, "\"float64\"")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(arginfo_ones, 0, 1, NDArray, 0)
    ZEND_ARG_TYPE_INFO(0, shape, IS_ARRAY, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, dtype, IS_STRING, 0, "\"float64\"")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(arginfo_full, 0, 2, NDArray, 0)
    ZEND_ARG_TYPE_INFO(0, shape, IS_ARRAY, 0)
    ZEND_ARG_INFO(0, value)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, dtype, IS_STRING, 0, "\"float64\"")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(arginfo_eye, 0, 1, NDArray, 0)
    ZEND_ARG_TYPE_INFO(0, n, IS_LONG, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, m, IS_LONG, 1, "null")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, k, IS_LONG, 0, "0")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, dtype, IS_STRING, 0, "\"float64\"")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(arginfo_arange, 0, 2, NDArray, 0)
    ZEND_ARG_INFO(0, start)
    ZEND_ARG_INFO(0, stop)
    ZEND_ARG_INFO_WITH_DEFAULT_VALUE(0, step, "1")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, dtype, IS_STRING, 1, "null")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(arginfo_fromArray, 0, 1, NDArray, 0)
    ZEND_ARG_TYPE_INFO(0, data, IS_ARRAY, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, dtype, IS_STRING, 1, "null")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_shape, 0, 0, IS_ARRAY, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_dtype, 0, 0, IS_STRING, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_size, 0, 0, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_ndim, 0, 0, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_toArray, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_offsetGet, 0, 1, IS_MIXED, 0)
    ZEND_ARG_INFO(0, offset)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_offsetSet, 0, 2, IS_VOID, 0)
    ZEND_ARG_INFO(0, offset)
    ZEND_ARG_INFO(0, value)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_offsetExists, 0, 1, _IS_BOOL, 0)
    ZEND_ARG_INFO(0, offset)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_offsetUnset, 0, 1, IS_VOID, 0)
    ZEND_ARG_INFO(0, offset)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_count, 0, 0, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(arginfo_slice, 0, 2, NDArray, 0)
    ZEND_ARG_TYPE_INFO(0, start, IS_LONG, 0)
    ZEND_ARG_TYPE_INFO(0, stop, IS_LONG, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, step, IS_LONG, 0, "1")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(arginfo_binary_op, 0, 2, NDArray, 0)
    ZEND_ARG_INFO(0, a)
    ZEND_ARG_INFO(0, b)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(arginfo_reshape, 0, 1, NDArray, 0)
    ZEND_ARG_TYPE_INFO(0, shape, IS_ARRAY, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(arginfo_transpose, 0, 0, NDArray, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, axes, IS_ARRAY, 1, "null")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(arginfo_flatten, 0, 0, NDArray, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(arginfo_squeeze, 0, 0, NDArray, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, axis, IS_LONG, 1, "null")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(arginfo_expandDims, 0, 1, NDArray, 0)
    ZEND_ARG_TYPE_INFO(0, axis, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(arginfo_concat, 0, 1, NDArray, 0)
    ZEND_ARG_TYPE_INFO(0, arrays, IS_ARRAY, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, axis, IS_LONG, 0, "0")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_dot, 0, 2, IS_DOUBLE, 0)
    ZEND_ARG_OBJ_INFO(0, a, NDArray, 0)
    ZEND_ARG_OBJ_INFO(0, b, NDArray, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(arginfo_matmul, 0, 2, NDArray, 0)
    ZEND_ARG_OBJ_INFO(0, a, NDArray, 0)
    ZEND_ARG_OBJ_INFO(0, b, NDArray, 0)
ZEND_END_ARG_INFO()

/* Reductions: ($axis = null, $keepdims = false) — return type is mixed because we
 * may return a scalar (axis omitted) or an NDArray (axis given or keepdims). */
ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_reduce, 0, 0, IS_MIXED, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, axis, IS_LONG, 1, "null")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, keepdims, _IS_BOOL, 0, "false")
ZEND_END_ARG_INFO()

/* var/std: (axis, keepdims, ddof) */
ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_var_std, 0, 0, IS_MIXED, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, axis, IS_LONG, 1, "null")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, keepdims, _IS_BOOL, 0, "false")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, ddof, IS_LONG, 0, "0")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(arginfo_unary, 0, 0, NDArray, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(arginfo_clip, 0, 2, NDArray, 0)
    ZEND_ARG_INFO_WITH_DEFAULT_VALUE(0, min, "null")
    ZEND_ARG_INFO_WITH_DEFAULT_VALUE(0, max, "null")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(arginfo_round, 0, 0, NDArray, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, decimals, IS_LONG, 0, "0")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(arginfo_power, 0, 1, NDArray, 0)
    ZEND_ARG_INFO(0, exponent)
ZEND_END_ARG_INFO()

/* sort/argsort: $axis defaults to -1 when omitted; explicit null = flatten.
 * The C handler uses the "axis_zv == NULL when omitted" trick to keep this
 * disambiguation in PHP land.  ZEND_ARG_TYPE_INFO with default null + nullable
 * makes the param formally optional. */
ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(arginfo_sort, 0, 0, NDArray, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, axis, IS_LONG, 1, "-1")
ZEND_END_ARG_INFO()

/* ===== method table + class registration ===== */

static const zend_function_entry numphp_ndarray_methods[] = {
    PHP_ME(NDArray, zeros,     arginfo_zeros,     ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_ME(NDArray, ones,      arginfo_ones,      ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_ME(NDArray, full,      arginfo_full,      ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_ME(NDArray, eye,       arginfo_eye,       ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_ME(NDArray, arange,    arginfo_arange,    ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_ME(NDArray, fromArray, arginfo_fromArray, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_ME(NDArray, shape,     arginfo_shape,     ZEND_ACC_PUBLIC)
    PHP_ME(NDArray, dtype,     arginfo_dtype,     ZEND_ACC_PUBLIC)
    PHP_ME(NDArray, size,      arginfo_size,      ZEND_ACC_PUBLIC)
    PHP_ME(NDArray, ndim,      arginfo_ndim,      ZEND_ACC_PUBLIC)
    PHP_ME(NDArray, toArray,   arginfo_toArray,   ZEND_ACC_PUBLIC)
    PHP_ME(NDArray, slice,        arginfo_slice,        ZEND_ACC_PUBLIC)
    PHP_ME(NDArray, add,          arginfo_binary_op,    ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_ME(NDArray, subtract,     arginfo_binary_op,    ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_ME(NDArray, multiply,     arginfo_binary_op,    ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_ME(NDArray, divide,       arginfo_binary_op,    ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_ME(NDArray, reshape,      arginfo_reshape,      ZEND_ACC_PUBLIC)
    PHP_ME(NDArray, transpose,    arginfo_transpose,    ZEND_ACC_PUBLIC)
    PHP_ME(NDArray, flatten,      arginfo_flatten,      ZEND_ACC_PUBLIC)
    PHP_ME(NDArray, squeeze,      arginfo_squeeze,      ZEND_ACC_PUBLIC)
    PHP_ME(NDArray, expandDims,   arginfo_expandDims,   ZEND_ACC_PUBLIC)
    PHP_ME(NDArray, concatenate,  arginfo_concat,       ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_ME(NDArray, stack,        arginfo_concat,       ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_ME(NDArray, dot,          arginfo_dot,          ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_ME(NDArray, matmul,       arginfo_matmul,       ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_ME(NDArray, inner,        arginfo_dot,          ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_ME(NDArray, outer,        arginfo_matmul,       ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_ME(NDArray, offsetGet,    arginfo_offsetGet,    ZEND_ACC_PUBLIC)
    PHP_ME(NDArray, offsetSet,    arginfo_offsetSet,    ZEND_ACC_PUBLIC)
    PHP_ME(NDArray, offsetExists, arginfo_offsetExists, ZEND_ACC_PUBLIC)
    PHP_ME(NDArray, offsetUnset,  arginfo_offsetUnset,  ZEND_ACC_PUBLIC)
    PHP_ME(NDArray, count,        arginfo_count,        ZEND_ACC_PUBLIC)

    /* reductions */
    PHP_ME(NDArray, sum,       arginfo_reduce,  ZEND_ACC_PUBLIC)
    PHP_ME(NDArray, mean,      arginfo_reduce,  ZEND_ACC_PUBLIC)
    PHP_ME(NDArray, min,       arginfo_reduce,  ZEND_ACC_PUBLIC)
    PHP_ME(NDArray, max,       arginfo_reduce,  ZEND_ACC_PUBLIC)
    PHP_ME(NDArray, var,       arginfo_var_std, ZEND_ACC_PUBLIC)
    PHP_ME(NDArray, std,       arginfo_var_std, ZEND_ACC_PUBLIC)
    PHP_ME(NDArray, argmin,    arginfo_reduce,  ZEND_ACC_PUBLIC)
    PHP_ME(NDArray, argmax,    arginfo_reduce,  ZEND_ACC_PUBLIC)
    PHP_ME(NDArray, nansum,    arginfo_reduce,  ZEND_ACC_PUBLIC)
    PHP_ME(NDArray, nanmean,   arginfo_reduce,  ZEND_ACC_PUBLIC)
    PHP_ME(NDArray, nanmin,    arginfo_reduce,  ZEND_ACC_PUBLIC)
    PHP_ME(NDArray, nanmax,    arginfo_reduce,  ZEND_ACC_PUBLIC)
    PHP_ME(NDArray, nanvar,    arginfo_var_std, ZEND_ACC_PUBLIC)
    PHP_ME(NDArray, nanstd,    arginfo_var_std, ZEND_ACC_PUBLIC)
    PHP_ME(NDArray, nanargmin, arginfo_reduce,  ZEND_ACC_PUBLIC)
    PHP_ME(NDArray, nanargmax, arginfo_reduce,  ZEND_ACC_PUBLIC)

    /* element-wise math (the trailing-underscore C names map to clean PHP names) */
    PHP_MALIAS(NDArray, sqrt,  sqrt_,  arginfo_unary, ZEND_ACC_PUBLIC)
    PHP_MALIAS(NDArray, exp,   exp_,   arginfo_unary, ZEND_ACC_PUBLIC)
    PHP_MALIAS(NDArray, log,   log_,   arginfo_unary, ZEND_ACC_PUBLIC)
    PHP_MALIAS(NDArray, log2,  log2_,  arginfo_unary, ZEND_ACC_PUBLIC)
    PHP_MALIAS(NDArray, log10, log10_, arginfo_unary, ZEND_ACC_PUBLIC)
    PHP_MALIAS(NDArray, abs,   abs_,   arginfo_unary, ZEND_ACC_PUBLIC)
    PHP_MALIAS(NDArray, floor, floor_, arginfo_unary, ZEND_ACC_PUBLIC)
    PHP_MALIAS(NDArray, ceil,  ceil_,  arginfo_unary, ZEND_ACC_PUBLIC)
    PHP_MALIAS(NDArray, round, round_, arginfo_round, ZEND_ACC_PUBLIC)
    PHP_ME(NDArray, clip,    arginfo_clip,  ZEND_ACC_PUBLIC)
    PHP_ME(NDArray, power,   arginfo_power, ZEND_ACC_PUBLIC)

    /* sort */
    PHP_ME(NDArray, sort,    arginfo_sort,  ZEND_ACC_PUBLIC)
    PHP_ME(NDArray, argsort, arginfo_sort,  ZEND_ACC_PUBLIC)

    PHP_FE_END
};

void numphp_register_ndarray_class(void)
{
    zend_class_entry ce;
    INIT_CLASS_ENTRY(ce, "NDArray", numphp_ndarray_methods);
    numphp_ndarray_ce = zend_register_internal_class(&ce);
    numphp_ndarray_ce->create_object = numphp_ndarray_create_object;

    zend_class_implements(numphp_ndarray_ce, 2,
        zend_ce_arrayaccess, zend_ce_countable);

    memcpy(&numphp_ndarray_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
    numphp_ndarray_handlers.offset          = XtOffsetOf(numphp_ndarray_object, std);
    numphp_ndarray_handlers.free_obj        = numphp_ndarray_free_object;
    numphp_ndarray_handlers.clone_obj       = numphp_ndarray_clone_object;
    numphp_ndarray_handlers.read_dimension  = numphp_offset_get;
    numphp_ndarray_handlers.write_dimension = numphp_offset_set;
    numphp_ndarray_handlers.has_dimension   = numphp_offset_has;
    numphp_ndarray_handlers.unset_dimension = numphp_offset_unset;
    numphp_ndarray_handlers.count_elements  = numphp_count_elements;
    numphp_ndarray_handlers.do_operation    = numphp_do_operation;
}

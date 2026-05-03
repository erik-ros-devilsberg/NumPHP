/* bufferview.c — BufferView class for FFI consumers.
 *
 * Lifetime: the BufferView holds a refcount on the source NDArray's underlying
 * numphp_buffer. This guarantees the buffer outlives the source array if the
 * user drops the array first. Story 2 designed `numphp_buffer` for exactly
 * this — refcount up on construction, refcount down on view destruction.
 *
 * Metadata (dtype/shape/strides/writeable/ptr) is captured ONCE at construction
 * time and exposed as public properties. The view is a snapshot — it does not
 * track subsequent reshape / transpose / mutation of the source.
 *
 * `$writeable` is advisory in v1: we expose the bool but do not enforce
 * read-only on the source. See decision 22 in docs/system.md.
 */

#include "bufferview.h"
#include "ndarray.h"

#include "Zend/zend_exceptions.h"

#include <stdint.h>
#include <string.h>

/* PHP_METHOD()-generated functions (zim_BufferView_*) are referenced only via
 * the zend_function_entry table by function pointer. See decision 36. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-prototypes"

zend_class_entry *numphp_bufferview_ce;
static zend_object_handlers numphp_bufferview_handlers;

typedef struct {
    numphp_buffer *buffer;     /* refcounted; we incremented in _create, decrement in _free */
    zend_object    std;
} numphp_bufferview_object;

static inline numphp_bufferview_object *bv_from_zo(zend_object *zo)
{
    return (numphp_bufferview_object *)((char *)zo - XtOffsetOf(numphp_bufferview_object, std));
}

static zend_object *numphp_bufferview_create_object(zend_class_entry *ce)
{
    numphp_bufferview_object *intern = zend_object_alloc(sizeof(numphp_bufferview_object), ce);
    zend_object_std_init(&intern->std, ce);
    object_properties_init(&intern->std, ce);
    intern->std.handlers = &numphp_bufferview_handlers;
    intern->buffer = NULL;
    return &intern->std;
}

static void numphp_bufferview_free_object(zend_object *zo)
{
    numphp_bufferview_object *intern = bv_from_zo(zo);
    if (intern->buffer) {
        numphp_buffer_release(intern->buffer);
        intern->buffer = NULL;
    }
    zend_object_std_dtor(&intern->std);
}

static zend_object *numphp_bufferview_clone_object(zend_object *zo)
{
    /* Clone produces a fresh BufferView pointing at the SAME underlying buffer
     * (with bumped refcount). Properties are copied via zend_objects_clone_members. */
    numphp_bufferview_object *src = bv_from_zo(zo);
    zend_object *new_zo = numphp_bufferview_create_object(zo->ce);
    numphp_bufferview_object *dst = bv_from_zo(new_zo);
    if (src->buffer) {
        numphp_buffer_addref(src->buffer);
        dst->buffer = src->buffer;
    }
    zend_objects_clone_members(&dst->std, &src->std);
    return new_zo;
}

int numphp_bufferview_create(zval *out, numphp_ndarray *a, int writeable)
{
    if (!(a->flags & NUMPHP_C_CONTIGUOUS)) {
        zend_throw_exception(numphp_ndarray_exception_ce,
            "NDArray::bufferView requires a C-contiguous source; "
            "call ->copy() (or clone) first to materialise a contiguous owner.", 0);
        return 0;
    }

    object_init_ex(out, numphp_bufferview_ce);
    numphp_bufferview_object *intern = bv_from_zo(Z_OBJ_P(out));

    /* Bump refcount on the source's buffer so it outlives the source NDArray. */
    numphp_buffer_addref(a->buffer);
    intern->buffer = a->buffer;

    /* Populate the public properties. We use zend_update_property_* — these
     * write through to the standard object properties table, so the values
     * are visible from PHP via $bv->dtype etc. */
    char *data_ptr = (char *)a->buffer->data + a->offset;

    zend_update_property_long(numphp_bufferview_ce, Z_OBJ_P(out),
        "ptr", sizeof("ptr") - 1, (zend_long)(uintptr_t)data_ptr);

    zend_update_property_string(numphp_bufferview_ce, Z_OBJ_P(out),
        "dtype", sizeof("dtype") - 1, numphp_dtype_name(a->dtype));

    zval shape_arr;
    array_init(&shape_arr);
    for (int i = 0; i < a->ndim; i++) {
        add_next_index_long(&shape_arr, (zend_long)a->shape[i]);
    }
    zend_update_property(numphp_bufferview_ce, Z_OBJ_P(out),
        "shape", sizeof("shape") - 1, &shape_arr);
    zval_ptr_dtor(&shape_arr);

    zval strides_arr;
    array_init(&strides_arr);
    for (int i = 0; i < a->ndim; i++) {
        add_next_index_long(&strides_arr, (zend_long)a->strides[i]);
    }
    zend_update_property(numphp_bufferview_ce, Z_OBJ_P(out),
        "strides", sizeof("strides") - 1, &strides_arr);
    zval_ptr_dtor(&strides_arr);

    zend_update_property_bool(numphp_bufferview_ce, Z_OBJ_P(out),
        "writeable", sizeof("writeable") - 1, writeable ? 1 : 0);

    return 1;
}

/* No public constructor — bufferView() is the only way to obtain an instance. */
PHP_METHOD(BufferView, __construct)
{
    ZEND_PARSE_PARAMETERS_NONE();
    zend_throw_exception(numphp_ndarray_exception_ce,
        "BufferView cannot be constructed directly; use NDArray::bufferView()", 0);
    RETURN_THROWS();
}

ZEND_BEGIN_ARG_INFO_EX(arginfo_bufferview_construct, 0, 0, 0)
ZEND_END_ARG_INFO()

static const zend_function_entry numphp_bufferview_methods[] = {
    PHP_ME(BufferView, __construct, arginfo_bufferview_construct, ZEND_ACC_PRIVATE)
    PHP_FE_END
};

void numphp_register_bufferview_class(void)
{
    zend_class_entry ce;
    INIT_CLASS_ENTRY(ce, "BufferView", numphp_bufferview_methods);
    numphp_bufferview_ce = zend_register_internal_class(&ce);
    numphp_bufferview_ce->create_object = numphp_bufferview_create_object;
    numphp_bufferview_ce->ce_flags |= ZEND_ACC_FINAL;

    /* Public properties — declared so reflection / var_dump see them.
     * Internal classes can't have refcounted defaults (arrays / strings), so
     * the array properties default to NULL and are populated by
     * numphp_bufferview_create() at construction time. */
    zend_declare_property_long(numphp_bufferview_ce,
        "ptr", sizeof("ptr") - 1, 0, ZEND_ACC_PUBLIC);
    zend_declare_property_string(numphp_bufferview_ce,
        "dtype", sizeof("dtype") - 1, "", ZEND_ACC_PUBLIC);
    zend_declare_property_null(numphp_bufferview_ce,
        "shape", sizeof("shape") - 1, ZEND_ACC_PUBLIC);
    zend_declare_property_null(numphp_bufferview_ce,
        "strides", sizeof("strides") - 1, ZEND_ACC_PUBLIC);
    zend_declare_property_bool(numphp_bufferview_ce,
        "writeable", sizeof("writeable") - 1, 0, ZEND_ACC_PUBLIC);

    /* Object handlers */
    memcpy(&numphp_bufferview_handlers, zend_get_std_object_handlers(),
           sizeof(zend_object_handlers));
    numphp_bufferview_handlers.offset    = XtOffsetOf(numphp_bufferview_object, std);
    numphp_bufferview_handlers.free_obj  = numphp_bufferview_free_object;
    numphp_bufferview_handlers.clone_obj = numphp_bufferview_clone_object;
}

#pragma GCC diagnostic pop

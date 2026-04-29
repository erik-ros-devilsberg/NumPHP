#include "numphp.h"

#include "ext/spl/spl_exceptions.h"
#include "ext/standard/info.h"
#include "ndarray.h"
#include "ops.h"
#include "linalg.h"
#include "nditer.h"

zend_class_entry *numphp_ndarray_exception_ce;
zend_class_entry *numphp_shape_exception_ce;
zend_class_entry *numphp_dtype_exception_ce;
zend_class_entry *numphp_index_exception_ce;

PHP_MINIT_FUNCTION(numphp)
{
    (void)type;
    (void)module_number;

    zend_class_entry ce;

    INIT_CLASS_ENTRY(ce, "NDArrayException", NULL);
    numphp_ndarray_exception_ce =
        zend_register_internal_class_ex(&ce, spl_ce_RuntimeException);

    INIT_CLASS_ENTRY(ce, "ShapeException", NULL);
    numphp_shape_exception_ce =
        zend_register_internal_class_ex(&ce, numphp_ndarray_exception_ce);

    INIT_CLASS_ENTRY(ce, "DTypeException", NULL);
    numphp_dtype_exception_ce =
        zend_register_internal_class_ex(&ce, numphp_ndarray_exception_ce);

    INIT_CLASS_ENTRY(ce, "IndexException", NULL);
    numphp_index_exception_ce =
        zend_register_internal_class_ex(&ce, numphp_ndarray_exception_ce);

    numphp_register_ndarray_class();

    return SUCCESS;
}

PHP_MINFO_FUNCTION(numphp)
{
    (void)zend_module;

    php_info_print_table_start();
    php_info_print_table_header(2, "numphp support", "enabled");
    php_info_print_table_row(2, "version", PHP_NUMPHP_VERSION);
    php_info_print_table_row(2, "thread-safety", "NTS only (v1)");
    php_info_print_table_end();
}

zend_module_entry numphp_module_entry = {
    STANDARD_MODULE_HEADER,
    "numphp",
    NULL,                  /* functions */
    PHP_MINIT(numphp),
    NULL,                  /* MSHUTDOWN */
    NULL,                  /* RINIT */
    NULL,                  /* RSHUTDOWN */
    PHP_MINFO(numphp),
    PHP_NUMPHP_VERSION,
    STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_NUMPHP
ZEND_GET_MODULE(numphp)
#endif

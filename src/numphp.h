#ifndef NUMPHP_H
#define NUMPHP_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"

#define PHP_NUMPHP_VERSION "0.0.13"

#ifdef ZTS
#error "numphp v1 does not support ZTS builds; rebuild PHP with --disable-zts."
#endif

extern zend_module_entry numphp_module_entry;
#define phpext_numphp_ptr &numphp_module_entry

extern zend_class_entry *numphp_ndarray_exception_ce;
extern zend_class_entry *numphp_shape_exception_ce;
extern zend_class_entry *numphp_dtype_exception_ce;
extern zend_class_entry *numphp_index_exception_ce;

#endif /* NUMPHP_H */

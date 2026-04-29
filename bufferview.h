#ifndef NUMPHP_BUFFERVIEW_H
#define NUMPHP_BUFFERVIEW_H

#include "numphp.h"
#include "ndarray.h"

extern zend_class_entry *numphp_bufferview_ce;

/* Construct a BufferView zval from `a`. Throws \NDArrayException if `a` is not
 * C-contiguous. Returns 1 on success (zval populated), 0 on failure (exception
 * thrown). The view holds a refcount on the array's underlying buffer so it
 * outlives the source NDArray. */
int numphp_bufferview_create(zval *out, numphp_ndarray *a, int writeable);

void numphp_register_bufferview_class(void);

#endif /* NUMPHP_BUFFERVIEW_H */

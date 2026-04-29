PHP_ARG_ENABLE([numphp],
  [whether to enable numphp support],
  [AS_HELP_STRING([--enable-numphp], [Enable numphp])])

if test "$PHP_NUMPHP" != "no"; then
  AC_MSG_CHECKING([whether PHP is built non-thread-safe])
  if test "$PHP_THREAD_SAFETY" = "yes"; then
    AC_MSG_RESULT([no])
    AC_MSG_ERROR([numphp v1 supports NTS only; ZTS is deferred. See docs/system.md.])
  fi
  AC_MSG_RESULT([yes])

  PHP_CHECK_LIBRARY(openblas, cblas_dgemm,
    [PHP_ADD_LIBRARY(openblas,, NUMPHP_SHARED_LIBADD)],
    [AC_MSG_ERROR([OpenBLAS (libopenblas) with cblas_dgemm not found])])

  PHP_CHECK_LIBRARY(lapack, dgetri_,
    [PHP_ADD_LIBRARY(lapack,, NUMPHP_SHARED_LIBADD)],
    [AC_MSG_ERROR([LAPACK (liblapack) with dgetri_ not found])])

  PHP_SUBST(NUMPHP_SHARED_LIBADD)

  PHP_NEW_EXTENSION(numphp,
    numphp.c ndarray.c ops.c linalg.c nditer.c,
    $ext_shared,, -DZEND_ENABLE_STATIC_TSRMLS_CACHE=1)
fi

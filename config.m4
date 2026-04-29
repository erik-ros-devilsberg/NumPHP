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

  dnl LAPACK symbol probe.
  dnl Linux convention: Fortran-mangled with trailing underscore (dgetri_).
  dnl macOS Accelerate / some BLAS distributions: no underscore (dgetri).
  dnl Try the underscored name first; fall back to the no-underscore name and
  dnl define NUMPHP_LAPACK_NO_USCORE so lapack_names.h aliases the symbols.
  PHP_CHECK_LIBRARY(lapack, dgetri_,
    [PHP_ADD_LIBRARY(lapack,, NUMPHP_SHARED_LIBADD)],
    [PHP_CHECK_LIBRARY(lapack, dgetri,
       [PHP_ADD_LIBRARY(lapack,, NUMPHP_SHARED_LIBADD)
        AC_DEFINE([NUMPHP_LAPACK_NO_USCORE], [1],
                  [Define if LAPACK symbols are exported without trailing underscore (e.g. macOS Accelerate)])],
       [AC_MSG_ERROR([LAPACK (liblapack) not found: tried dgetri_ and dgetri])])])

  PHP_SUBST(NUMPHP_SHARED_LIBADD)

  PHP_NEW_EXTENSION(numphp,
    numphp.c ndarray.c ops.c linalg.c nditer.c io.c,
    $ext_shared,, -DZEND_ENABLE_STATIC_TSRMLS_CACHE=1)
fi

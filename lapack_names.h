#ifndef NUMPHP_LAPACK_NAMES_H
#define NUMPHP_LAPACK_NAMES_H

/* Single source of truth for LAPACK Fortran symbol names.
 *
 * The Linux / reference-LAPACK convention is to export Fortran-mangled symbols
 * with a trailing underscore (`dgetri_`). macOS Accelerate exports the same
 * routines without the underscore (`dgetri`). config.m4 detects which form is
 * present and defines NUMPHP_LAPACK_NO_USCORE for the no-underscore case.
 *
 * All call sites in linalg.c write the underscored name; this header rewrites
 * to the bare name when needed. ALL LAPACK symbols Story 10 touches must appear
 * here — adding a new routine requires adding a line here. */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#ifdef NUMPHP_LAPACK_NO_USCORE
#  define dgetrf_ dgetrf
#  define dgetri_ dgetri
#  define dgesv_  dgesv
#  define dgesdd_ dgesdd
#  define dgeev_  dgeev
#  define dlange_ dlange
#  define dnrm2_  dnrm2
#  define sgetrf_ sgetrf
#  define sgetri_ sgetri
#  define sgesv_  sgesv
#  define sgesdd_ sgesdd
#  define sgeev_  sgeev
#  define slange_ slange
#  define snrm2_  snrm2
#endif

/* LAPACK uses Fortran integer (typically int32 on most platforms). Prototype
 * what we use; we deliberately keep this minimal and grow it as the linalg
 * module needs more routines. */
typedef int numphp_lapack_int;

#ifdef __cplusplus
extern "C" {
#endif

/* LU factorisation */
void dgetrf_(numphp_lapack_int *m, numphp_lapack_int *n, double *a,
             numphp_lapack_int *lda, numphp_lapack_int *ipiv, numphp_lapack_int *info);
void sgetrf_(numphp_lapack_int *m, numphp_lapack_int *n, float *a,
             numphp_lapack_int *lda, numphp_lapack_int *ipiv, numphp_lapack_int *info);

/* Inverse from LU */
void dgetri_(numphp_lapack_int *n, double *a, numphp_lapack_int *lda,
             numphp_lapack_int *ipiv, double *work, numphp_lapack_int *lwork,
             numphp_lapack_int *info);
void sgetri_(numphp_lapack_int *n, float *a, numphp_lapack_int *lda,
             numphp_lapack_int *ipiv, float *work, numphp_lapack_int *lwork,
             numphp_lapack_int *info);

/* General linear solver A x = b */
void dgesv_(numphp_lapack_int *n, numphp_lapack_int *nrhs, double *a,
            numphp_lapack_int *lda, numphp_lapack_int *ipiv, double *b,
            numphp_lapack_int *ldb, numphp_lapack_int *info);
void sgesv_(numphp_lapack_int *n, numphp_lapack_int *nrhs, float *a,
            numphp_lapack_int *lda, numphp_lapack_int *ipiv, float *b,
            numphp_lapack_int *ldb, numphp_lapack_int *info);

/* Divide-and-conquer SVD */
void dgesdd_(char *jobz, numphp_lapack_int *m, numphp_lapack_int *n, double *a,
             numphp_lapack_int *lda, double *s, double *u, numphp_lapack_int *ldu,
             double *vt, numphp_lapack_int *ldvt, double *work, numphp_lapack_int *lwork,
             numphp_lapack_int *iwork, numphp_lapack_int *info);
void sgesdd_(char *jobz, numphp_lapack_int *m, numphp_lapack_int *n, float *a,
             numphp_lapack_int *lda, float *s, float *u, numphp_lapack_int *ldu,
             float *vt, numphp_lapack_int *ldvt, float *work, numphp_lapack_int *lwork,
             numphp_lapack_int *iwork, numphp_lapack_int *info);

/* Non-symmetric eigenvalue / eigenvector */
void dgeev_(char *jobvl, char *jobvr, numphp_lapack_int *n, double *a,
            numphp_lapack_int *lda, double *wr, double *wi,
            double *vl, numphp_lapack_int *ldvl,
            double *vr, numphp_lapack_int *ldvr,
            double *work, numphp_lapack_int *lwork, numphp_lapack_int *info);
void sgeev_(char *jobvl, char *jobvr, numphp_lapack_int *n, float *a,
            numphp_lapack_int *lda, float *wr, float *wi,
            float *vl, numphp_lapack_int *ldvl,
            float *vr, numphp_lapack_int *ldvr,
            float *work, numphp_lapack_int *lwork, numphp_lapack_int *info);

/* Matrix norms (1, inf, max-abs, Frobenius) */
double dlange_(char *norm, numphp_lapack_int *m, numphp_lapack_int *n,
               double *a, numphp_lapack_int *lda, double *work);
float slange_(char *norm, numphp_lapack_int *m, numphp_lapack_int *n,
              float *a, numphp_lapack_int *lda, float *work);

/* Vector 2-norm (Euclidean) */
double dnrm2_(numphp_lapack_int *n, double *x, numphp_lapack_int *incx);
float snrm2_(numphp_lapack_int *n, float *x, numphp_lapack_int *incx);

#ifdef __cplusplus
}
#endif

#endif /* NUMPHP_LAPACK_NAMES_H */

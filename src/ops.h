#ifndef NUMPHP_OPS_H
#define NUMPHP_OPS_H

#include "numphp.h"
#include "ndarray.h"

/* Reduction op selector. Welford is used for VAR / STD; pairwise sum for SUM / MEAN. */
typedef enum {
    NUMPHP_REDUCE_SUM = 0,
    NUMPHP_REDUCE_MEAN,
    NUMPHP_REDUCE_MIN,
    NUMPHP_REDUCE_MAX,
    NUMPHP_REDUCE_VAR,
    NUMPHP_REDUCE_STD,
    NUMPHP_REDUCE_ARGMIN,
    NUMPHP_REDUCE_ARGMAX,
} numphp_reduce_op;

/* Reduce `src` along `axis` (or globally if has_axis = 0).
 *   has_axis = 0           → global reduction (output is 0-D, or all-1 dims if keepdims)
 *   has_axis = 1, axis k   → reduce dim k (negative axis allowed at PHP layer; pass normalised k)
 *   keepdims               → preserve reduced axis as size 1 in output shape
 *   ddof                   → only used by VAR / STD
 *   skip_nan               → NaN-aware variant (nansum, nanmean, ...)
 *
 * Returns a fresh owner ndarray on success, NULL with an exception thrown on error.
 * Caller is responsible for freeing the result (or wrapping it via numphp_zval_wrap_ndarray).
 */
numphp_ndarray *numphp_reduce(numphp_ndarray *src, numphp_reduce_op op,
                              int has_axis, int axis, int keepdims, int ddof, int skip_nan);

/* Cumulative reductions — running sum / running product along an axis. */
typedef enum {
    NUMPHP_CUM_SUM = 0,
    NUMPHP_CUM_PROD,
} numphp_cumulative_op;

/* Cumulate `src` along `axis` (or globally if has_axis = 0).
 *   has_axis = 0  → flatten input, output is 1-D of length src->size
 *   has_axis = 1  → cumulate along `axis`, output shape == input shape
 *   skip_nan      → NaN-aware variant (treat NaN as identity: 0 for sum, 1 for prod)
 *
 * Output dtype rules (locked by docs/system.md decision 31):
 *   int32 / int64 → int64; float32 → float32; float64 → float64
 *
 * Returns a fresh owner ndarray on success, NULL with an exception on error.
 */
numphp_ndarray *numphp_cumulative(numphp_ndarray *src, numphp_cumulative_op op,
                                  int has_axis, int axis, int skip_nan);

/* Element-wise math ops. All return a freshly allocated C-contiguous owner. */
typedef enum {
    NUMPHP_MATH_SQRT = 0,
    NUMPHP_MATH_EXP,
    NUMPHP_MATH_LOG,
    NUMPHP_MATH_LOG2,
    NUMPHP_MATH_LOG10,
    NUMPHP_MATH_ABS,
    NUMPHP_MATH_FLOOR,
    NUMPHP_MATH_CEIL,
} numphp_math_op;

numphp_ndarray *numphp_apply_unary(numphp_ndarray *src, numphp_math_op op);
numphp_ndarray *numphp_apply_clip(numphp_ndarray *src, int has_min, double minv, int has_max, double maxv);
numphp_ndarray *numphp_apply_round(numphp_ndarray *src, int decimals);
numphp_ndarray *numphp_apply_power_scalar(numphp_ndarray *src, double exponent);

/* Sort and argsort. axis = INT_MIN sentinel for "axis === null" (flatten then sort 1-D). */
#define NUMPHP_AXIS_FLATTEN (-2147483648)

numphp_ndarray *numphp_sort(numphp_ndarray *src, int axis);
numphp_ndarray *numphp_argsort(numphp_ndarray *src, int axis);

#endif /* NUMPHP_OPS_H */

<?php
/**
 * Ordinary least-squares linear regression.
 *
 * Recovers β in y = β_0 + β_1 x from synthetic data generated with known
 * coefficients. Solves the normal equations (XᵀX) β = Xᵀy via Linalg::solve.
 *
 * No noise is added so the recovered coefficients are exact to machine
 * precision — a small tolerance check confirms recovery.
 */

$n = 50;
$true_intercept = 3.0;
$true_slope = 2.0;

$x = NDArray::arange(0, $n, 1, 'float64');
$y = NDArray::add(NDArray::multiply($x, $true_slope), $true_intercept);

$ones = NDArray::ones([$n], 'float64');
$X = NDArray::stack([$ones, $x], 1);

$Xt = $X->transpose();
$XtX = NDArray::matmul($Xt, $X);
$Xty = NDArray::matmul($Xt, $y->reshape([$n, 1]));

$beta = Linalg::solve($XtX, $Xty)->flatten();

$beta_arr = $beta->toArray();
printf("β_0 (intercept) = %.6f\n", $beta_arr[0]);
printf("β_1 (slope)     = %.6f\n", $beta_arr[1]);

$residual = abs($beta_arr[0] - $true_intercept) + abs($beta_arr[1] - $true_slope);
echo $residual < 1e-9 ? "OK\n" : "FAIL: residual = $residual\n";

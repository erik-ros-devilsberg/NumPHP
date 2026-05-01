<?php
/**
 * Simple time-series operations on a 1-D NDArray.
 *
 * Generates 30 daily values (a sine wave + linear drift), computes a
 * 7-day rolling mean via slice + mean, demonstrates NaN-aware reductions
 * by injecting holes via slice + offsetSet, and proves view-vs-copy by
 * mutating a slice and observing the change in the source array.
 */

$n = 30;
// daily values: y_t = 10 + 0.2·t + 3·sin(2πt/7)
$series_data = [];
for ($t = 0; $t < $n; $t++) {
    $series_data[] = 10.0 + 0.2 * $t + 3.0 * sin(2 * M_PI * $t / 7);
}
$series = NDArray::fromArray($series_data);
echo "series dtype: ", $series->dtype(), "\n";
printf("first 7 days: %s\n", json_encode(
    array_map(fn($v) => round($v, 3), array_slice($series_data, 0, 7))
));

// 7-day rolling mean: window of size 7, output length n-6.
$window = 7;
$means = [];
for ($i = 0; $i + $window <= $n; $i++) {
    $means[] = $series->slice($i, $i + $window)->mean();
}
$rolling = NDArray::fromArray($means);
printf("rolling.shape = (%d,)\n", $rolling->shape()[0]);
printf("rolling[0]   = %.4f\n", $rolling->toArray()[0]);
printf("rolling[-1]  = %.4f\n", $rolling->toArray()[$rolling->shape()[0] - 1]);

// view-vs-copy: slice produces a view; mutating the view writes through.
$first_week = $series->slice(0, 7);
printf("before mutation, series[3] = %.4f\n", $series->toArray()[3]);
$first_week[3] = 999.0;
printf("after first_week[3] = 999, series[3] = %.4f\n", $series->toArray()[3]);
// restore so the NaN-aware demo below has clean input
$first_week[3] = $series_data[3];

// inject NaN at indices 5 and 11; demonstrate that mean propagates NaN
// while nanmean ignores them.
$series[5] = NAN;
$series[11] = NAN;
echo "mean(series)    = ", $series->mean(), "\n";    // expect NaN
echo "nanmean(series) = ", round($series->nanmean(), 4), "\n";
echo "OK\n";

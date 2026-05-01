<?php
/**
 * Lloyd's algorithm k-means on a synthetic 2-D dataset.
 *
 * Three well-separated Gaussian-ish clusters (deterministic samples around
 * fixed centres, no RNG so the .expected file is stable). The algorithm
 * iterates assignment + recompute until the assignment vector stops changing.
 *
 * Demonstrates: broadcasting (distance via X[:,None,:] − C[None,:,:]),
 * argmin along an axis, and per-cluster mean via boolean-style indexing
 * implemented as masked-by-multiply (since fancy indexing is post-v1).
 */

$true_centres = [
    [0.0,  0.0],
    [10.0, 0.0],
    [5.0,  9.0],
];

$points_per_cluster = 8;
$rows = [];
foreach ($true_centres as $c) {
    for ($i = 0; $i < $points_per_cluster; $i++) {
        // deterministic offset pattern: small grid around the centre
        $dx = (($i % 4) - 1.5) * 0.3;
        $dy = ((intdiv($i, 4)) - 0.5) * 0.3;
        $rows[] = [$c[0] + $dx, $c[1] + $dy];
    }
}
$X = NDArray::fromArray($rows);   // shape (24, 2)
$n = $X->shape()[0];
$k = 3;

// initial centroids: spread across the dataset (one row per true cluster) so
// the example shows convergence rather than a stuck local minimum. Real code
// would use k-means++; that's a tangent for an example focused on broadcasting.
$init_indices = [0, $points_per_cluster, 2 * $points_per_cluster];
$centroids_arr = [];
foreach ($init_indices as $i) {
    $centroids_arr[] = $rows[$i];
}
$centroids = NDArray::fromArray($centroids_arr);

$prev_assignments = null;
$max_iter = 20;
for ($iter = 0; $iter < $max_iter; $iter++) {
    // distance² = sum over axis=2 of (X[:,None,:] - C[None,:,:])²
    $X_expanded = $X->reshape([$n, 1, 2]);
    $C_expanded = $centroids->reshape([1, $k, 2]);
    $diff = NDArray::subtract($X_expanded, $C_expanded);
    $sq = NDArray::multiply($diff, $diff);
    $dist_sq = $sq->sum(2);             // shape (n, k)
    $assignments = $dist_sq->argmin(1);  // shape (n,)

    $assign_arr = $assignments->toArray();
    if ($prev_assignments === $assign_arr) {
        break;
    }
    $prev_assignments = $assign_arr;

    // recompute centroids
    $new_centroid_rows = [];
    for ($c = 0; $c < $k; $c++) {
        $sx = 0.0; $sy = 0.0; $count = 0;
        foreach ($assign_arr as $idx => $a) {
            if ($a === $c) {
                $sx += $rows[$idx][0];
                $sy += $rows[$idx][1];
                $count++;
            }
        }
        $new_centroid_rows[] = $count > 0
            ? [$sx / $count, $sy / $count]
            : $centroids_arr[$c];
    }
    $centroids = NDArray::fromArray($new_centroid_rows);
}

printf("converged after %d iter%s\n", $iter, $iter === 1 ? '' : 's');
foreach ($centroids->toArray() as $i => $c) {
    printf("cluster %d centroid: (%.3f, %.3f)\n", $i, $c[0], $c[1]);
}
echo "OK\n";

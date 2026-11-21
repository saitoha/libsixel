#!/bin/sh
# Keep GPU palette construction tied to the pipeline's SIMD geometry.
set -eux
echo '1..1'
set -v

awk '
FILENAME == ARGV[1] {
    if (/simd_width[[:space:]]+\[\[threads_per_simdgroup\]\]/) width = 1
    if (/g = lane; g < groups; g \+= simd_width/) reduction = 1
    if (/^#define[[:space:]]+HB_SIMD[[:space:]]/) fixed = 1
}
FILENAME == ARGV[2] {
    if (/simd_width = .*threadExecutionWidth;/) pipeline = 1
    if (/threads -= threads % simd_width;/) aligned = 1
    if (/threads = 32u \* simd_width;/) bounded = 1
    if (/if \(!heckbert_gpu_result_valid\(/) validated = 1
    if (/memcpy\(request->order,|\*request->box_count =/) {
        if (!validated) unchecked = 1
        published = 1
    }
}
END {
    exit !(width && reduction && !fixed && pipeline && aligned && bounded &&
        validated && published && !unchecked)
}
' "$1/src/palette-heckbert-gpu.metal" "$1/src/palette-heckbert-gpu-metal.m" || {
    echo 'not ok 1 - Metal Heckbert solver respects SIMD width and validates its result'
    exit 0
}
echo 'ok 1 - Metal Heckbert solver respects SIMD width and validates its result'
exit 0

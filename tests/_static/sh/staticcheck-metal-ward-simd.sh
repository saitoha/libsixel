#!/bin/sh
# Keep both Ward dispatch paths and the shader tied to runtime SIMD widths.
set -eux
echo '1..1'
set -v

awk '
FILENAME == ARGV[1] {
    if (/\(\(count \+ width - 1u\) \/ width\) \* width/) aligned = 1
    if (/width < 16u \|\| width > limit/) minimum = 1
}
FILENAME == ARGV[2] {
    if (/lanes = sixel_ward_metal_merge_lanes\(count,/) direct = 1
    if (/sixel_ward_metal_merge\.threadExecutionWidth/) direct_merge = 1
    if (/sixel_ward_metal_rows\.threadExecutionWidth, 1, 1/) direct_rows = 1
    direct_source = direct_source " " $0
}
FILENAME == ARGV[3] {
    if (/ward_merge_lanes = sixel_ward_metal_merge_lanes\(/) resident = 1
    if (/WD_MERGE\]\.threadExecutionWidth/) resident_merge = 1
    if (/WD_ROWS\]\.threadExecutionWidth/) resident_rows = 1
    if (/RUN\(WD_ROWS, geometry.colors \* ward_rows_width, ward_rows_width,/) {
        resident_row_dispatch = 1
    }
    if (/RUN\(WD_MERGE, ward_merge_lanes, ward_merge_lanes,/) {
        resident_merge_dispatch = 1
    }
    resident_source = resident_source " " $0
}
FILENAME == ARGV[4] {
    if (/^ward_merge\(/) merge = 1
    if (/^#define WARD_GROUP_CAPACITY .*\/ 16u/) capacity = 1
    if (/minima\[WARD_GROUP_CAPACITY\]/) minima = 1
    if (/peers\[WARD_GROUP_CAPACITY\]/) peers = 1
    if (merge && /width \[\[threads_per_simdgroup\]\]/) shader_width = 1
    if (/group_best.peer % width/) shuffle = 1
    if (/index = lane; index < groups; index \+= width/) reduction = 1
    if (/candidate \+= width/) repair = 1
}
END {
    fixed = direct_source ~ /sixel_ward_metal_(rows|merge)\.threadExecutionWidth[[:space:]]*!=[[:space:]]*32/ ||
        resident_source ~ /WD_(ROWS|MERGE)\]\.threadExecutionWidth[[:space:]]*!=[[:space:]]*32/
    exit !(aligned && minimum && direct && direct_merge && direct_rows &&
        resident && resident_merge && resident_rows && resident_row_dispatch &&
        resident_merge_dispatch && capacity && minima && peers &&
        shader_width && shuffle && reduction && repair && !fixed)
}
' "$1/src/palette-common-merge.h" "$1/src/palette-ward-metal.m" \
    "$1/src/palette-kmeans-metal.m" "$1/src/palette-ward.metal" || {
    echo 'not ok 1 - Metal Ward dispatch and reduction respect SIMD widths'
    exit 0
}
echo 'ok 1 - Metal Ward dispatch and reduction respect SIMD widths'
exit 0

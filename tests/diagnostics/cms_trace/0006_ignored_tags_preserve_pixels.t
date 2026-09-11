#!/bin/sh
# Policy: docs/loader/color-management.md
# Observe builtin ICC decisions without changing the image load result.

set -eux

printf '1..1\n'
set -v

input="${TOP_SRCDIR}/tests/data/colormgmt/input/custom/rgb_trace_ignored_tags.png"

value=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" --cms-engine=builtin -L "builtin!" -m MS-SSIM --env SIXEL_TRACE_TOPIC=loader "$input" "${TOP_SRCDIR}/tests/data/colormgmt/input/custom/rgb_parametric_012.png") || {
    echo "not ok 1 - profiled image must remain loadable"
    exit 0
}

test "$value" = 1.000000 || {
    echo "not ok 1 - ignored tags must preserve normalized pixels"
    exit 0
}

echo "ok 1 - ignored tags preserve normalized pixels"
exit 0

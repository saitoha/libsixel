#!/bin/sh
# Policy: docs/loader/color-management/README.md
# Observe builtin ICC decisions without changing the image load result.

set -eux

printf '1..1\n'
set -v

input="${TOP_SRCDIR}/tests/data/colormgmt/input/custom/rgb_trace_ignored_tags.png"

message=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" --cms-engine=builtin -L "builtin!" -m MS-SSIM --env SIXEL_TRACE_TOPIC= "$input" "$input" 2>&1 1>/dev/null) || {
    printf '\n%s\n' "not ok 1 - profiled image must remain loadable"
    exit 0
}

test -z "$message" || {
    printf '\n%s\n' "not ok 1 - disabled trace must remain quiet"
    exit 0
}

printf '\n%s\n' "ok 1 - disabled trace remains quiet"
exit 0

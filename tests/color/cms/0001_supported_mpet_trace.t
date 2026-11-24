#!/bin/sh
# Policy: docs/loader/builtin-cms.md
# Policy: docs/loader/color-management/README.md
# Observe builtin ICC decisions without changing the image load result.

set -eux

printf '1..1\n'
set -v

input="${TOP_SRCDIR}/tests/data/colormgmt/input/custom/rgb_trace_ignored_tags.png"

message=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" --cms-engine=builtin -L "builtin!" -m MS-SSIM --env SIXEL_TRACE_TOPIC=loader "$input" "$input" 2>&1 1>/dev/null) || {
    printf '\n%s\n' "not ok 1 - profiled image must remain loadable"
    exit 0
}

case "${message}" in
    *tag=44324233*|*event=pipeline-rejected\|tag=D2B3*) false ;;
    *) true ;;
esac || {
    printf '\n%s\n' "not ok 1 - supported D2B3 must not be ignored or rejected"
    exit 0
}

printf '\n%s\n' "ok 1 - supported D2B3 must not be ignored or rejected"
exit 0

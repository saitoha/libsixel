#!/bin/sh
# Policy: docs/loader/builtin-cms.md
# Policy: docs/loader/color-management.md
# Observe builtin ICC decisions without changing the image load result.

set -eux

printf '1..1\n'
set -v

input="${TOP_SRCDIR}/tests/data/colormgmt/input/custom/rgb_trace_ignored_tags.png"

message=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" --cms-engine=builtin -L "builtin!" -m MS-SSIM --env SIXEL_TRACE_TOPIC=loader "$input" "$input" 2>&1 1>/dev/null) || {
    echo "not ok 1 - profiled image must remain loadable"
    exit 0
}

test "${message#*tag=44324230|type=6d706574|reason=unsupported-pipeline-type}" != "$message" || {
    echo "not ok 1 - missing ICC decision reason"
    exit 0
}

echo "ok 1 - mpet payload reason"
exit 0

#!/bin/sh
# Confirm prefixed PNG output is created via png: scheme.
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

echo "1..1"
set -v

SIXEL_LOG_PATH="${TOP_BUILDDIR}/sixel-timeline-codec-format-0001.log"
export SIXEL_LOG_PATH

snake_jpg="${TOP_SRCDIR}/tests/data/inputs/snake_64.jpg"
prefixed_png="${ARTIFACT_LOCAL_DIR}/snake-prefixed.png"

run_img2sixel -o "png:${prefixed_png}" "${snake_jpg}" || {
    fail 1 "prefixed PNG conversion failed"
    exit 0
}

test -s "${prefixed_png}" || {
    fail 1 "prefixed PNG output missing"
    exit 0
}

pass 1 "prefixed PNG output created"

exit 0

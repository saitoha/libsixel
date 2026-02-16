#!/bin/sh
# Confirm prefixed PNG output is created via png: scheme.
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

config_macro_defined HAVE_IMG2SIXEL || skip_all "img2sixel is disabled in this build"

echo "1..1"
set -v

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

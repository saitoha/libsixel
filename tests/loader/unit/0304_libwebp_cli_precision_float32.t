#!/bin/sh
# Policy: docs/encoder/precision/README.md
# Verify the CLI precision request reaches a selectable byte loader.
set -eux
echo "1..1"
set -v
test "${HAVE_WEBP-}" = 1 && test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "ok 1 # SKIP img2sixel or libwebp unavailable"
    exit 0
}
trace=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --threading-policy=1 \
    -L 'libwebp:cms_engine=none!' -.float32 -Xgamma -Wgamma -dnone -p2 \
    -v -o/dev/null "${TOP_SRCDIR}/tests/data/inputs/snake_64.webp" 2>&1) || {
    echo "not ok 1 - precision conversion failed"
    exit 0
}
case "$trace" in *"source=rgb-f32 work=rgb-f32"*) true ;; *) false ;; esac || {
    echo "$trace" >&2
    echo "not ok 1 - 0304_libwebp_cli_precision_float32"
    exit 0
}
echo "ok 1 - 0304_libwebp_cli_precision_float32"
exit 0

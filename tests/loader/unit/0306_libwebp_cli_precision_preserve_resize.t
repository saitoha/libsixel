#!/bin/sh
# Policy: docs/encoder/precision/README.md
# Verify preserve resizing retains an eligible byte decoding path.
set -eux
echo "1..1"
set -v
test "${HAVE_WEBP-}" = 1 && test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "ok 1 # SKIP img2sixel or libwebp unavailable"
    exit 0
}
trace=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --threading-policy=1 \
    -L 'libwebp:cms_engine=none!' -.8bit -w32 -j scalar:resize_precision=preserve -Xgamma -Wgamma -dnone -p2 \
    -v -o/dev/null "${TOP_SRCDIR}/tests/data/inputs/snake_64.webp" 2>&1) || {
    echo "not ok 1 - precision conversion failed"
    exit 0
}
case "$trace" in *"source=rgb888 work=rgb888"*) true ;; *) false ;; esac || {
    echo "$trace" >&2
    echo "not ok 1 - 0306_libwebp_cli_precision_preserve_resize"
    exit 0
}
echo "ok 1 - 0306_libwebp_cli_precision_preserve_resize"
exit 0

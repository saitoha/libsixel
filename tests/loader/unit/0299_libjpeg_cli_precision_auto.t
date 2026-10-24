#!/bin/sh
# Policy: docs/encoder/precision/README.md
# Verify the CLI precision request reaches a selectable byte loader.
set -eux
echo "1..1"
set -v
test "${HAVE_JPEG-}" = 1 && test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "ok 1 # SKIP img2sixel or libjpeg unavailable"
    exit 0
}
trace=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --threading-policy=balanced:global_budget=1 \
    -L 'libjpeg:cms_engine=none!' -.auto -Xgamma -Wgamma -dnone -p2 \
    -v -o/dev/null "${TOP_SRCDIR}/tests/data/inputs/formats/snake-jpeg-8bit-rgb-seq444.jpg" 2>&1) || {
    echo "not ok 1 - precision conversion failed"
    exit 0
}
case "$trace" in *"source=rgb888 work=rgb888"*) true ;; *) false ;; esac || {
    echo "$trace" >&2
    echo "not ok 1 - 0299_libjpeg_cli_precision_auto"
    exit 0
}
echo "ok 1 - 0299_libjpeg_cli_precision_auto"
exit 0

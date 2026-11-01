#!/bin/sh
# High-color preserves tRNS masks and requests keep with either loader route.
# Policy: docs/encoder/output/high-color/README.md
set -eux

test "${HAVE_LIBPNG-}" = 1 || {
    echo "1..0 # SKIP libpng support is disabled in this build"
    exit 0
}
test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled in this build"
    exit 0
}

echo "1..1"
set -v

keycolor_header="$(printf '\033P0;1q')"
out_on_payload="$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Akeep -I \
    --threading-policy=greedy:global_budget=1 --env SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=1 \
    -Llibpng:cms_engine=none! \
    "${TOP_SRCDIR}/images/pngsuite/transparency/tbbn0g04.png")" || {
    echo "not ok 1 - highcolor opt-in render failed"
    exit 0
}
out_off_payload="$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Akeep -I \
    --threading-policy=greedy:global_budget=1 --env SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=0 \
    -Llibpng:cms_engine=none! \
    "${TOP_SRCDIR}/images/pngsuite/transparency/tbbn0g04.png")" || {
    echo "not ok 1 - highcolor opt-out render failed"
    exit 0
}

case "${out_on_payload}" in
    "${keycolor_header}"*) ;;
    *)
        echo "not ok 1 - highcolor opt-in lost the keep header"
        exit 0
        ;;
esac
case "${out_off_payload}" in
    "${keycolor_header}"*) ;;
    *)
        echo "not ok 1 - highcolor opt-out lost the keep header"
        exit 0
        ;;
esac

echo "ok 1 - highcolor preserves tRNS masks through both loader routes"
exit 0

#!/bin/sh
# Verify short -% env override enables keycolor over process opt-out.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_png="${TOP_SRCDIR}/images/pngsuite/transparency/tbbn0g04.png"
out="${ARTIFACT_LOCAL_DIR}/builtin-trns-keycolor-cli-short-override-tbbn0g04.six"

SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=0 \
    run_img2sixel -% SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=1 \
                  -Lbuiltin:cms_engine=none! -d fs -y raster \
                  "${input_png}" >"${out}" || {
    echo "not ok 1 - builtin process env=0 + -%=1 render failed"
    exit 0
}

case "$(cat "${out}")" in
    *"$(printf '\033')P0;1q"*)
        echo "ok 1 - builtin -%=1 overrides process env=0"
        ;;
    *)
        echo "not ok 1 - builtin -%=1 did not override process env=0"
        ;;
esac

exit 0

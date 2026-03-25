#!/bin/sh
# Verify scan=serpentine + carry keeps keycolor header without external scripting helpers.

set -eux

test "${HAVE_LIBPNG-}" = 1 || {
    echo "1..0 # SKIP libpng support is disabled in this build"
    exit 0
}

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled in this build"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_png="${TOP_SRCDIR}/images/pngsuite/transparency/tbbn0g04.png"
out="${ARTIFACT_LOCAL_DIR}/trns-keycolor-scan-serpentine-carry-f32-lso2-tbbn0g04.six"

run_img2sixel --env SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=1 \
              --env SIXEL_THREADS=4 \
              -Llibpng:cms=0! \
              --precision=float32 \
              -d lso2 -y serpentine -Y carry \
              "${input_png}" >"${out}" || {
    echo "not ok 1 - scan/carry render failed"
    exit 0
}

case "$(cat "${out}")" in
    *"$(printf '\033')P0;1q"*)
        echo "ok 1 - scan/carry keeps keycolor header"
        ;;
    *)
        echo "not ok 1 - scan/carry lost keycolor header"
        ;;
esac

exit 0

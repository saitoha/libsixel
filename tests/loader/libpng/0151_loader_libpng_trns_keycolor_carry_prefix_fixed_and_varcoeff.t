#!/bin/sh
# Verify carry-enabled varcoeff keeps keycolor header for a tRNS keycolor sample.

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
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_png="${TOP_SRCDIR}/images/pngsuite/transparency/tbbn0g04.png"
output_six="${ARTIFACT_LOCAL_DIR}/trns-keycolor-lso2-carry-tbbn0g04.six"
${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=1 \
              --env SIXEL_THREADS=4 \
              -Llibpng:cms_engine=none! \
              -d lso2:scan=raster \
              "${input_png}" >"${output_six}" || {
    echo "not ok 1 - carry varcoeff render failed"
    exit 0
}

set +x
output_six_text=""
IFS= read -r output_six_text < "${output_six}" || test -n "${output_six_text}"
case "${output_six_text}" in
    *"$(printf '\033')P0;1q"*)
        echo "ok 1 - carry varcoeff keeps keycolor header"
        ;;
    *)
        echo "not ok 1 - carry varcoeff lost keycolor header"
        ;;
esac

exit 0

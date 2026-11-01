#!/bin/sh
# TAP test confirming librsvg respects explicit width and height geometry.

set -eux

test "${HAVE_LIBRSVG-}" = 1 || {
    printf "1..0 # SKIP librsvg loader is unavailable in this build\n"
    exit 0
}

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

svg_path="${TOP_SRCDIR}/tests/data/inputs/formats/librsvg-explicit-size.svg"
sixel_path="${ARTIFACT_LOCAL_DIR}/librsvg-explicit-size.six"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L librsvg! "${svg_path}" >"${sixel_path}" || {
    echo "not ok" 1 - "librsvg explicit-size conversion failed"
    exit 0
}

IFS='"' read -r _ raster _ <"${sixel_path}" || :
test -n "${raster-}" || {
    echo "not ok" 1 - "failed to read sixel raster for explicit geometry"
    exit 0
}
raster="${raster%%#*}"
case "${raster}" in
    "1;1;13;7")
        ;;
    *)
        echo "not ok" 1 - "explicit width/height geometry is not 13x7"
        exit 0
        ;;
esac

echo "ok" 1 - "librsvg explicit width/height geometry is respected"
exit 0

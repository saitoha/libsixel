#!/bin/sh
# TAP test confirming librsvg uses viewBox geometry when size is omitted.

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

svg_path="${TOP_SRCDIR}/tests/data/inputs/formats/librsvg-viewbox-size.svg"
sixel_path="${ARTIFACT_LOCAL_DIR}/librsvg-viewbox-size.six"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L librsvg! "${svg_path}" >"${sixel_path}" || {
    echo "not ok" 1 - "librsvg viewBox conversion failed"
    exit 0
}

IFS='"' read -r _ raster _ <"${sixel_path}" || :
test -n "${raster-}" || {
    echo "not ok" 1 - "failed to read sixel raster for viewBox geometry"
    exit 0
}
raster="${raster%%#*}"
case "${raster}" in
    "1;1;40;25")
        ;;
    *)
        echo "not ok" 1 - "viewBox geometry is not 40x25"
        exit 0
        ;;
esac

echo "ok" 1 - "librsvg viewBox geometry is respected"
exit 0

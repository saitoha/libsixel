#!/bin/sh
# TAP test confirming librsvg -B option composites transparent SVG output.

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

svg_path="${TOP_SRCDIR}/tests/data/inputs/formats/librsvg-empty-2x1.svg"
sixel_path="${ARTIFACT_LOCAL_DIR}/librsvg-bgcolor-white.six"
esc="$(printf '\033')"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L librsvg! -B '#ffffff' "${svg_path}" \
    >"${sixel_path}" || {
    echo "not ok" 1 - "white background conversion failed"
    exit 0
}

IFS= read -r sixel_line <"${sixel_path}" || :
test -n "${sixel_line-}" || {
    echo "not ok" 1 - "failed to read sixel output header"
    exit 0
}
case "${sixel_line}" in
    "${esc}Pq"*)
        ;;
    *)
        echo "not ok" 1 - "background option did not emit opaque SIXEL header"
        exit 0
        ;;
esac

echo "ok" 1 - "background option composites transparent SVG output"
exit 0

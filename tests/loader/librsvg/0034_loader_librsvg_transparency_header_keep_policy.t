#!/bin/sh
# TAP test confirming keep transparent policy emits the P2=1 SIXEL header.

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

svg_path="${TOP_SRCDIR}/tests/data/inputs/formats/librsvg-transparent-2color.svg"
keep_sixel="${ARTIFACT_LOCAL_DIR}/librsvg-transparent-keep-header.six"
esc="$(printf '\033')"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --transparent-policy=keep \
    -L librsvg! "${svg_path}" >"${keep_sixel}" || {
    echo "not ok" 1 - "keep transparent SVG conversion failed"
    exit 0
}

IFS= read -r sixel_line <"${keep_sixel}" || :
test -n "${sixel_line-}" || {
    echo "not ok" 1 - "failed to read keep transparent SVG SIXEL header"
    exit 0
}
case "${sixel_line}" in
    "${esc}P0;1q"*)
        ;;
    *)
        echo "not ok" 1 - "keep transparent SVG did not emit ESC P0;1q header"
        exit 0
        ;;
esac

echo "ok" 1 - "keep transparent SVG emits P2=1 SIXEL header"
exit 0

#!/bin/sh
# TAP test confirming opt-in enables relative external resource decode.

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

svg_path="${TOP_SRCDIR}/tests/data/inputs/formats/librsvg-relative-image.svg"
optin_sixel="${ARTIFACT_LOCAL_DIR}/librsvg-relative-optin.six"
esc="$(printf '\033')"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env SIXEL_LOADER_LIBRSVG_ALLOW_RELATIVE_RESOURCES=1 \
    -L librsvg! "${svg_path}" >"${optin_sixel}" || {
    echo "not ok" 1 - "opt-in relative-resource SVG conversion failed"
    exit 0
}

IFS= read -r sixel_line <"${optin_sixel}" || :
test -n "${sixel_line-}" || {
    echo "not ok" 1 - "failed to read opt-in relative-resource SIXEL header"
    exit 0
}
case "${sixel_line}" in
    "${esc}Pq"*)
        ;;
    *)
        echo "not ok" 1 - "opt-in path did not resolve relative resource"
        exit 0
        ;;
esac

echo "ok" 1 - "opt-in enables relative resource decode"
exit 0

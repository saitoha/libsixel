#!/bin/sh
# TAP test confirming stdin .svgz decode succeeds when opt-in is set for librsvg.

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

svgz_path="${TOP_SRCDIR}/tests/data/inputs/formats/librsvg-transparent-2color.svgz"
stdin_optin_sixel="${ARTIFACT_LOCAL_DIR}/librsvg-svgz-stdin-optin.six"
esc="$(printf '\033')"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env SIXEL_LOADER_LIBRSVG_ALLOW_STDIN_SVGZ=1 \
    -L librsvg! - \
    >"${stdin_optin_sixel}" \
    <"${svgz_path}" || {
    echo "not ok" 1 - "stdin .svgz conversion failed with opt-in env"
    exit 0
}

IFS= read -r sixel_line <"${stdin_optin_sixel}" || :
test -n "${sixel_line-}" || {
    echo "not ok" 1 - "failed to read stdin .svgz opt-in SIXEL header"
    exit 0
}
case "${sixel_line}" in
    "${esc}P0;0q"*)
        ;;
    *)
        echo "not ok" 1 - "stdin .svgz opt-in conversion lost transparency header"
        exit 0
        ;;
esac

echo "ok" 1 - "librsvg stdin .svgz decode works with opt-in"
exit 0

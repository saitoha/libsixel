#!/bin/sh
# Verify SIXEL_LOADER_BACKGROUND_COLORSPACE defaults to gamma in builtin path.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}


echo "1..1"
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_png="${TOP_SRCDIR}/images/pngsuite/background/bgan6a08.png"
output_default="${ARTIFACT_LOCAL_DIR}/builtin_bgcs_default.six"
output_gamma="${ARTIFACT_LOCAL_DIR}/builtin_bgcs_gamma.six"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Lbuiltin:cms_engine=none! -B#808080 "${input_png}" >"${output_default}" || {
    echo "not ok 1 - builtin default background colorspace conversion failed"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env SIXEL_LOADER_BACKGROUND_COLORSPACE=gamma \
              -Lbuiltin:cms_engine=none! \
              -B#808080 "${input_png}" >"${output_gamma}" || {
    echo "not ok 1 - builtin gamma background colorspace conversion failed"
    exit 0
}

cmp -s "${output_default}" "${output_gamma}" || {
    echo "not ok 1 - builtin default background colorspace does not match gamma"
    exit 0
}

echo "ok 1 - builtin default background colorspace matches gamma"

exit 0

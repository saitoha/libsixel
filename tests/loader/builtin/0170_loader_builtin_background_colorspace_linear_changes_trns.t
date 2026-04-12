#!/bin/sh
# Verify linear background interpretation changes tRNS composition in builtin path.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}


echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_png="${TOP_SRCDIR}/tests/data/inputs/formats/rgba.png"
output_gamma="${ARTIFACT_LOCAL_DIR}/builtin_bgcs_trns_gamma.six"
output_linear="${ARTIFACT_LOCAL_DIR}/builtin_bgcs_trns_linear.six"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env SIXEL_LOADER_BACKGROUND_COLORSPACE=gamma \
              -Lbuiltin:cms_engine=none! \
              -B#808080 "${input_png}" >"${output_gamma}" || {
    echo "not ok 1 - builtin RGBA gamma background composition failed"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env SIXEL_LOADER_BACKGROUND_COLORSPACE=linear \
              -Lbuiltin:cms_engine=none! \
              -B#808080 "${input_png}" >"${output_linear}" || {
    echo "not ok 1 - builtin RGBA linear background composition failed"
    exit 0
}

cmp -s "${output_gamma}" "${output_linear}" && {
    echo "not ok 1 - builtin gamma and linear RGBA composition produced identical output"
    exit 0
}

echo "ok 1 - builtin linear background interpretation changes RGBA composition"
exit 0

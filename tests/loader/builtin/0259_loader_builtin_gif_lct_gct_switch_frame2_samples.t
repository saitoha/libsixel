#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/gif.md
# TAP test: LCT/GCT switching animation keeps frame2 colors stable.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}


printf '1..1\n'
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_gif="${TOP_SRCDIR}/tests/data/inputs/formats/gif-lct-gct-switch-anim.gif"
reference_png="${TOP_SRCDIR}/tests/data/inputs/formats/gif-lct-gct-switch-frame2-reference.png"
output_six="${ARTIFACT_LOCAL_DIR}/builtin_gif_lct_gct_switch_frame2.six"
normalized_six="${ARTIFACT_LOCAL_DIR}/builtin_gif_lct_gct_switch_frame2_norm.six"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env SIXEL_THREADING_GLOBAL_BUDGET=4 \
              --env SIXEL_LOADER_ANIMATION_START_FRAME_NO=1 \
              -Lbuiltin! -ldisable -d none:scan=raster -p 256 -g \
              "${input_gif}" >"${output_six}" || {
    echo "not ok" 1 - "LCT/GCT switch GIF frame2 decode failed"
    exit 0
}

# The animation writer prefixes this selected frame with CUP home. Verify
# that prefix before removing it for the standalone SIXEL loader.
prefix=$(dd if="${output_six}" bs=3 count=1 2>/dev/null)
test "${prefix}" = "$(printf '\033[H')" || {
    echo "not ok 1 - selected animation frame has an unexpected prefix"
    exit 0
}
dd if="${output_six}" of="${normalized_six}" bs=1 skip=3 2>/dev/null || {
    echo "not ok 1 - selected animation frame normalization failed"
    exit 0
}

# Palette switching must preserve every decoded component exactly.
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/image_values \
    "${reference_png}" "${normalized_six}" 'builtin:Enone!' 0 || {
    echo "not ok 1 - selected GIF frame values differ from the reference"
    exit 0
}

echo "ok 1 - selected GIF frame values match the reference"
exit 0

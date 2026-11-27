#!/bin/sh
# Verify that auto resolves a non-inverse LUT pair to the full stage.
# Policy: docs/encoder/palette-application/target-color-policy.md
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}
echo "1..1"
set -v

test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

data="${TOP_SRCDIR}/tests/data/color-numeric"
# Gamma sRGB bytes have themselves as targets under an sRGB -U.
srgb_image="${data}/cli-rgb.png"
# Constant A2B/B2A tables: every code reproduces one color, so the
# encode/interpret pair is not an inverse while all codes stay in range.
lut_output="icc:path=${data}/relative-paired-lut.icc"
out="${ARTIFACT_LOCAL_DIR}/0048-target-$$"

# Gamut-only correction cannot see a mismatch hidden inside [0,1] codes;
# auto must measure it from source colors and resolve to full.
${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" "-U${lut_output}" -Ygamut -dfs -p8 "${srgb_image}" \
    >"${out}-gamut.six" || {
    printf '\n%s\n' "not ok 1 - gamut policy conversion failed"
    exit 0
}
${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" "-U${lut_output}" -Yfull -dfs -p8 "${srgb_image}" \
    >"${out}-full.six" || {
    printf '\n%s\n' "not ok 1 - full policy conversion failed"
    exit 0
}
${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" "-U${lut_output}" -Yauto -dfs -p8 "${srgb_image}" \
    >"${out}-auto.six" || {
    printf '\n%s\n' "not ok 1 - auto policy conversion failed"
    exit 0
}
cmp -s "${out}-gamut.six" "${out}-full.six" && {
    printf '\n%s\n' "not ok 1 - gamut and full agree on a non-inverse LUT"
    exit 0
}
cmp -s "${out}-full.six" "${out}-auto.six" || {
    printf '\n%s\n' "not ok 1 - auto did not resolve a non-inverse LUT to full"
    exit 0
}
printf '\n%s\n' "ok 1 - auto resolves a non-inverse LUT pair to full"
exit 0

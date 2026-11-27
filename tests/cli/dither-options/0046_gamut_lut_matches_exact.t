#!/bin/sh
# Verify that a gamut-stage target LUT matches exact evaluation.
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
# Display P3 primaries lie outside the default sRGB -U gamut.
p3_image="${data}/cg-expanded-p3-binary.png"
out="${ARTIFACT_LOCAL_DIR}/0046-target-$$"

# In-gamut LUT nodes carry zero offsets and boundaries stay exact.
${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Ygamut -dfs -p4 "${p3_image}" \
    >"${out}-gamut.six" || {
    printf '\n%s\n' "not ok 1 - exact gamut conversion failed"
    exit 0
}
${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Ygamut:Mlut:S5 -dfs -p4 "${p3_image}" \
    >"${out}-gamut_lut.six" || {
    printf '\n%s\n' "not ok 1 - gamut LUT conversion failed"
    exit 0
}
cmp -s "${out}-gamut.six" "${out}-gamut_lut.six" || {
    printf '\n%s\n' "not ok 1 - gamut LUT differs from exact evaluation"
    exit 0
}
printf '\n%s\n' "ok 1 - gamut LUT matches exact evaluation"
exit 0

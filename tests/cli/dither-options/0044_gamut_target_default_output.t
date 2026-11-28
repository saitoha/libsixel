#!/bin/sh
# Verify that explicit default gamut target suboptions keep the output.
# Policy: docs/encoder/output/gamut-mapping.md
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
out="${ARTIFACT_LOCAL_DIR}/0044-gamut-target-$$"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -dfs -p4 "${p3_image}" \
    >"${out}-default.six" || {
    printf '\n%s\n' "not ok 1 - default conversion failed"
    exit 0
}
${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -U srgb:diffusion=input:samples=mapped -dfs -p4 "${p3_image}" \
    >"${out}-explicit.six" || {
    printf '\n%s\n' "not ok 1 - explicit default conversion failed"
    exit 0
}
test -s "${out}-default.six" || {
    printf '\n%s\n' "not ok 1 - default output is empty"
    exit 0
}
cmp -s "${out}-default.six" "${out}-explicit.six" || {
    printf '\n%s\n' "not ok 1 - explicit defaults changed the output"
    exit 0
}
printf '\n%s\n' "ok 1 - explicit gamut target defaults keep the output"
exit 0

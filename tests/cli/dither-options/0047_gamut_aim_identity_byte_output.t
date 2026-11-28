#!/bin/sh
# Verify that an identity -U keeps byte sRGB output under gamut aims.
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
# Gamma sRGB bytes are their own aims under an sRGB -U.
srgb_image="${data}/cli-rgb.png"
out="${ARTIFACT_LOCAL_DIR}/0047-gamut-aim-$$"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -dfs -p8 "${srgb_image}" \
    >"${out}-default.six" || {
    printf '\n%s\n' "not ok 1 - default conversion failed"
    exit 0
}
${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -U srgb:diffusion=aim:samples=shown -dfs -p8 "${srgb_image}" \
    >"${out}-aim.six" || {
    printf '\n%s\n' "not ok 1 - gamut aim conversion failed"
    exit 0
}
cmp -s "${out}-default.six" "${out}-aim.six" || {
    printf '\n%s\n' "not ok 1 - identity -U changed byte sRGB output"
    exit 0
}
printf '\n%s\n' "ok 1 - identity -U keeps byte sRGB output"
exit 0

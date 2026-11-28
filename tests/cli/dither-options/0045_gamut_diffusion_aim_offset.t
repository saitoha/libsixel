#!/bin/sh
# Verify that diffusion=aim diffuses toward the -U gamut aim.
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
out="${ARTIFACT_LOCAL_DIR}/0045-gamut-aim-$$"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -dfs -p4 "${p3_image}" \
    >"${out}-default.six" || {
    printf '\n%s\n' "not ok 1 - default conversion failed"
    exit 0
}
trace=$(set +xv; SIXEL_TRACE_TOPIC=dither_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -U srgb:diffusion=aim:samples=shown -dfs -p4 \
    "${p3_image}" 2>&1 >"${out}-gamut.six") || {
    printf '\n%s\n' "not ok 1 - gamut aim conversion failed"
    exit 0
}
case "${trace}" in *LSXDTH1\|*gamut_diffusion=1\|gamut_samples=1\|*gamut_aim_offset=1*) true ;; *) false ;; esac || {
    printf '\n%s\n' "not ok 1 - diffusion=aim built no aim offset"
    exit 0
}
cmp -s "${out}-default.six" "${out}-gamut.six" && {
    printf '\n%s\n' "not ok 1 - gamut aim did not change out-of-gamut feedback"
    exit 0
}
printf '\n%s\n' "ok 1 - diffusion=aim diffuses toward the -U gamut aim"
exit 0

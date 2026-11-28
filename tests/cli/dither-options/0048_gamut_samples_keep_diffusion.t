#!/bin/sh
# Verify that samples=shown alone keeps diffusion on the mapped input.
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
out="${ARTIFACT_LOCAL_DIR}/0048-gamut-samples-$$"

trace=$(set +xv; SIXEL_TRACE_TOPIC=dither_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -U srgb:samples=shown -dfs -p4 \
    "${p3_image}" 2>&1 >"${out}.six") || {
    printf '\n%s\n' "not ok 1 - samples=shown conversion failed"
    exit 0
}
case "${trace}" in *LSXDTH1\|*gamut_diffusion=0\|gamut_samples=1\|*gamut_aim_offset=0*) true ;; *) false ;; esac || {
    printf '\n%s\n' "not ok 1 - samples=shown changed the diffusion input"
    exit 0
}
printf '\n%s\n' "ok 1 - samples=shown keeps diffusion on the mapped input"
exit 0

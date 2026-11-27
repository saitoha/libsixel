#!/bin/sh
# Verify that byte error diffusion rejects a target with a diagnostic.
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
out="${ARTIFACT_LOCAL_DIR}/0049-target-$$"

status=0
message=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --precision-policy=8bit -Yfull -dfs -p4 "${p3_image}" \
    2>&1 >"${out}-byte.six") || status=$?
test "${status}" -ne 0 || {
    printf '\n%s\n' "not ok 1 - byte diffusion accepted a target"
    exit 0
}
case "${message}" in *"target-color-policy requires float32 error diffusion"*) true ;; *) false ;; esac || {
    printf '\n%s\n' "not ok 1 - byte diffusion rejection lacks its diagnostic"
    exit 0
}
printf '\n%s\n' "ok 1 - byte diffusion rejects a target with a diagnostic"
exit 0

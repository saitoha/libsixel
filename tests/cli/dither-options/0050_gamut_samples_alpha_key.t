#!/bin/sh
# Verify that alpha-key byte clustering samples take gamut aims.
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
# Transparent RGBA keeps byte clustering samples for the alpha key.
alpha_image="${data}/icc-cluster-alpha.png"
out="${ARTIFACT_LOCAL_DIR}/0050-gamut-samples-$$"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -U smpte-c:samples=shown -dfs -p8 "${alpha_image}" \
    >"${out}-alpha.six" || {
    printf '\n%s\n' "not ok 1 - alpha-key byte samples rejected a gamut aim"
    exit 0
}
test -s "${out}-alpha.six" || {
    printf '\n%s\n' "not ok 1 - alpha-key output is empty"
    exit 0
}
printf '\n%s\n' "ok 1 - alpha-key byte samples take gamut aims"
exit 0

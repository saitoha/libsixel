#!/bin/sh
# Verify that alpha-key byte clustering samples take targets.
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
# Transparent RGBA keeps byte clustering samples for the alpha key.
alpha_image="${data}/icc-cluster-alpha.png"
out="${ARTIFACT_LOCAL_DIR}/0050-target-$$"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Usmpte-c -Yfull -dfs -p8 "${alpha_image}" \
    >"${out}-alpha.six" || {
    printf '\n%s\n' "not ok 1 - alpha-key byte samples rejected a target"
    exit 0
}
test -s "${out}-alpha.six" || {
    printf '\n%s\n' "not ok 1 - alpha-key output is empty"
    exit 0
}
printf '\n%s\n' "ok 1 - alpha-key byte samples take targets"
exit 0

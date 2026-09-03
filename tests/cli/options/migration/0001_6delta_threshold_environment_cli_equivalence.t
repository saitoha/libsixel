#!/bin/sh
# Preserve SIXEL_6DELTA_THRESHOLD equivalence with -Z during migration.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input="${TOP_SRCDIR}/tests/data/inputs/formats/apng_8x8_rgba_loop2.png"
cli_output="${ARTIFACT_LOCAL_DIR}/6delta-threshold-cli-$$.six"
env_output="${ARTIFACT_LOCAL_DIR}/6delta-threshold-env-$$.six"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -l disable -Z 32 -Y skip \
    "${input}" >"${cli_output}" || {
    echo "not ok 1 - 6delta threshold CLI conversion failed"
    exit 0
}
${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -l disable \
    --env SIXEL_6DELTA_THRESHOLD=32 -Y skip "${input}" \
    >"${env_output}" || {
    echo "not ok 1 - 6delta threshold environment conversion failed"
    exit 0
}
cmp -s "${cli_output}" "${env_output}" || {
    echo "not ok 1 - 6delta threshold environment and CLI differ"
    exit 0
}

echo "ok 1 - 6delta threshold environment matches CLI"
exit 0

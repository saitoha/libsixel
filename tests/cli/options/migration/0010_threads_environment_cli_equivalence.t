#!/bin/sh
# Preserve SIXEL_THREADS equivalence with -= during migration.

set -eux

test "${HAVE_SIXEL2PNG-}" = 1 || {
    printf "1..0 # SKIP sixel2png is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input="${TOP_SRCDIR}/images/map64.six"
cli_output="${ARTIFACT_LOCAL_DIR}/threads-cli-$$.png"
env_output="${ARTIFACT_LOCAL_DIR}/threads-env-$$.png"

${SIXEL_RUNTIME-} "${SIXEL2PNG_PATH}" -= 1 -D <"${input}" \
    >"${cli_output}" || {
    echo "not ok 1 - threads CLI conversion failed"
    exit 0
}
${SIXEL_RUNTIME-} "${SIXEL2PNG_PATH}" --env SIXEL_THREADS=1 -D \
    <"${input}" >"${env_output}" || {
    echo "not ok 1 - threads environment conversion failed"
    exit 0
}
cmp -s "${cli_output}" "${env_output}" || {
    echo "not ok 1 - threads environment and CLI differ"
    exit 0
}

echo "ok 1 - threads environment matches CLI"
exit 0

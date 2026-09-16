#!/bin/sh
# Preserve the historical rule that SIXEL_COLORS=1 is ignored.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input="${TOP_SRCDIR}/tests/data/inputs/snake_16.png"
default_output="${ARTIFACT_LOCAL_DIR}/colors-default-$$.six"
env_output="${ARTIFACT_LOCAL_DIR}/colors-one-$$.six"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" "${input}" \
    >"${default_output}" || {
    echo "not ok 1 - default colors conversion failed"
    exit 0
}
${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env SIXEL_COLORS=1 "${input}" \
    >"${env_output}" || {
    echo "not ok 1 - SIXEL_COLORS=1 conversion failed"
    exit 0
}
cmp -s "${default_output}" "${env_output}" || {
    echo "not ok 1 - SIXEL_COLORS=1 changed the output"
    exit 0
}

echo "ok 1 - SIXEL_COLORS=1 remains ignored"
exit 0

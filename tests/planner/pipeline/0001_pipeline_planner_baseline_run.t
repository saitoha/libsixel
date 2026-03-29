#!/bin/sh
# TAP test: pipeline planner runs baseline case with verbose dump.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

ppm_small="${TOP_SRCDIR}/tests/data/inputs/small.ppm"


${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env SIXEL_THREADS=4 -v -o "${ARTIFACT_LOCAL_DIR}/small.six" "${ppm_small}" \
        >"${ARTIFACT_LOCAL_DIR}/small.out" || {
    printf 'not ok 1 - pipeline run failed (baseline)\n'
    exit 0
}

printf 'ok 1 - pipeline run succeeded (baseline)\n'
exit 0

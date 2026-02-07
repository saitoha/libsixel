#!/bin/sh
# TAP test: pipeline planner runs baseline case with verbose dump.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

ppm_small="${TOP_SRCDIR}/tests/data/inputs/small.ppm"

echo "1..1"
set -v

run_img2sixel --env SIXEL_THREADS=4 -v -o "${ARTIFACT_LOCAL_DIR}/small.six" "${ppm_small}" \
        >"${ARTIFACT_LOCAL_DIR}/small.out" || {
    printf 'not ok 1 - pipeline run failed (baseline)\n'
    exit 0
}

printf 'ok 1 - pipeline run succeeded (baseline)\n'
exit 0

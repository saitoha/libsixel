#!/bin/sh
# TAP test: pipeline planner runs baseline case with verbose dump.

set -eux

export SIXEL_THREADS=4

ppm_small="${ARTIFACT_LOCAL_DIR}/grid_small.ppm"


script_dir=$(CDPATH=; cd "${0%[/\\]*}" && pwd)
. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"
. "${TOP_SRCDIR}/tests/lib/sh/pipeline/pipeline_planner_common.sh"

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

echo "1..1"
set -v

create_small_ppm "${ppm_small}"

if run_img2sixel -v -o "${ARTIFACT_LOCAL_DIR}/small.six" "${ppm_small}" \
        >"${ARTIFACT_LOCAL_DIR}/small.out"; then
    printf 'ok 1 - pipeline run succeeded (baseline)\n'
else
    printf 'not ok 1 - pipeline run failed (baseline)\n'
    exit 1
fi

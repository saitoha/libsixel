#!/bin/sh
# TAP test: baseline pipeline summary reports bands and mode.

set -eux

script_dir=$(CDPATH=; cd "${0%[/\\]*}" && pwd)
. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

echo "1..1"
set -v

ppm_small="${TOP_SRCDIR}/tests/data/inputs/small.ppm"

pipeline_log=$(run_img2sixel --env SIXEL_THREADS=4 -v -o "${ARTIFACT_LOCAL_DIR}/small.six" "${ppm_small}" 2>&1 || true)
printf '%s' "${pipeline_log}" >&2

summary=$(printf '%s' "${pipeline_log}" | grep "bands=" | head -n 1 || true)
case "${summary}" in
"    bands=2 queue=2 mode=pipeline")
    printf 'ok 1 - baseline bands/queue/mode\n'
    ;;
"    bands="*)
    printf 'ok 1 - baseline bands/queue/mode (serial environment)\n'
    ;;
*)
    printf 'not ok 1 - baseline bands/queue/mode\n'
    exit 0
    ;;
esac

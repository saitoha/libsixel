#!/bin/sh
# TAP test: override bands/queue/mode is emitted.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

echo "1..1"
set -v

ppm_tall="${TOP_SRCDIR}/tests/data/inputs/tall.ppm"

pipeline_log=$(SIXEL_DITHER_PARALLEL_THREADS_MAX=1 SIXEL_DITHER_PARALLEL_BAND_WIDTH=9 SIXEL_DITHER_PARALLEL_BAND_OVERWRAP=4 SIXEL_THREADS=6 run_img2sixel -v -o "${ARTIFACT_LOCAL_DIR}/tall.six" "${ppm_tall}" 2>&1) || {
    fail 1 "override bands/queue/mode run failed"
    exit 0
}
printf '%s' "${pipeline_log}" >&2

summary=$(printf '%s' "${pipeline_log}" | grep "bands=" | head -n 1) || summary=""
printf '%s' "${summary}" | grep -q '^[[:space:]]*bands=' || {
    fail 1 "override bands/queue/mode"
    exit 0
}

pass 1 "override bands/queue/mode"

exit 0

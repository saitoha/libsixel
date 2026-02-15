#!/bin/sh
# TAP test: override bands/queue/mode is emitted.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

echo "1..1"
set -v

ppm_tall="${TOP_SRCDIR}/tests/data/inputs/tall.ppm"

pipeline_log=$(
    run_img2sixel --env SIXEL_DITHER_PARALLEL_THREADS_MAX=1 \
                  --env SIXEL_DITHER_PARALLEL_BAND_WIDTH=9 \
                  --env SIXEL_DITHER_PARALLEL_BAND_OVERWRAP=4 \
                  --env SIXEL_THREADS=6 \
                  -v -o "${ARTIFACT_LOCAL_DIR}/tall.six" "${ppm_tall}" 2>&1) || {
    fail 1 "override bands/queue/mode run failed"
    exit 0
}
printf '%s' "${pipeline_log}" >&2

printf '%s\n' "${pipeline_log}" | awk '/bands=/{ found = 1; exit } END{ if (!found) exit 1 }' || {
    fail 1 "override bands/queue/mode"
    exit 0
}

pass 1 "override bands/queue/mode"
exit 0

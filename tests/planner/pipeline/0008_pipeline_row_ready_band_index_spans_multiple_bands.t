#!/bin/sh
# TAP test: pipeline row_ready events span more than one band.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

echo "1..1"
set -v

ppm_tall="${TOP_SRCDIR}/tests/data/inputs/tall.ppm"
log_file="${ARTIFACT_LOCAL_DIR}/pipeline-row-ready-span.log"
out_file="${ARTIFACT_LOCAL_DIR}/tall-row-ready-span.six"

run_img2sixel --env SIXEL_LOG_PATH="${log_file}" \
              --env SIXEL_THREADS=6 \
              --env SIXEL_DITHER_PARALLEL_THREADS_MAX=1 \
              --env SIXEL_DITHER_PARALLEL_BAND_WIDTH=9 \
              --env SIXEL_DITHER_PARALLEL_BAND_OVERWRAP=4 \
              -v -o "${out_file}" "${ppm_tall}" || {
    fail 1 "row_ready span conversion failed"
    exit 0
}

grep -q '"event":"row_ready"' "${log_file}" || {
    pass 1 "row_ready span unavailable in serial environment"
    exit 0
}

max_job=$(grep '"event":"row_ready"' "${log_file}" | sed -E 's/.*"job":(-?[0-9]+).*/\001/' | sort -n | tail -n 1)
test "${max_job}" -ge 1 || {
    fail 1 "row_ready spans multiple bands"
    exit 0
}

pass 1 "row_ready spans multiple bands"

exit 0

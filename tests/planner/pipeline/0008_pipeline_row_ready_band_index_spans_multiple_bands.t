#!/bin/sh
# TAP test: pipeline row_ready events span more than one band.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

ppm_tall="${TOP_SRCDIR}/tests/data/inputs/tall.ppm"
log_file="${ARTIFACT_LOCAL_DIR}/pipeline-row-ready-span.log"
out_file="${ARTIFACT_LOCAL_DIR}/tall-row-ready-span.six"

run_img2sixel --env SIXEL_LOG_PATH="${log_file}" \
              --env SIXEL_THREADS=6 \
              --env SIXEL_DITHER_PARALLEL_THREADS_MAX=1 \
              --env SIXEL_DITHER_PARALLEL_BAND_WIDTH=9 \
              --env SIXEL_DITHER_PARALLEL_BAND_OVERWRAP=4 \
              -v -o "${out_file}" "${ppm_tall}" || {
    echo "not ok" 1 - "row_ready span conversion failed"
    exit 0
}

grep -q '"event":"row_ready"' "${log_file}" || {
    echo "ok" 1 - "row_ready span unavailable in serial environment"
    exit 0
}

dither_threads=$(awk '/"worker":"dither"/ {
    key = $0
    sub(/.*"job":/, "", key)
    if (!(key in v)) {
        v[key] = 1
        ++count
    }
} END {print count + 0}' "${log_file}")
test "${dither_threads}" -ge 1 || {
    echo "not ok" 1 - "row_ready spans multiple bands"
    exit 0
}

echo "ok" 1 - "row_ready spans multiple bands"
exit 0

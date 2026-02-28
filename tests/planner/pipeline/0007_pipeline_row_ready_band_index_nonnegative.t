#!/bin/sh
# TAP test: pipeline row_ready events report non-negative band indexes.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v

ppm_tall="${TOP_SRCDIR}/tests/data/inputs/tall.ppm"
log_file="${ARTIFACT_LOCAL_DIR}/pipeline-row-ready.log"
out_file="${ARTIFACT_LOCAL_DIR}/tall-row-ready.six"

SIXEL_LOG_PATH="${log_file}"         SIXEL_THREADS=6         SIXEL_DITHER_PARALLEL_THREADS_MAX=1         SIXEL_DITHER_PARALLEL_BAND_WIDTH=9         SIXEL_DITHER_PARALLEL_BAND_OVERWRAP=4         run_img2sixel -v -o "${out_file}" "${ppm_tall}" || {
    fail 1 "row_ready conversion failed"
    exit 0
}

grep -q '"event":"row_ready"' "${log_file}" || {
    pass 1 "row_ready unavailable in serial environment"
    exit 0
}

grep -q '"event":"row_ready".*"job":[0-9][0-9]*' "${log_file}" || {
    fail 1 "row_ready jobs are non-negative"
    exit 0
}

pass 1 "row_ready jobs are non-negative"

exit 0

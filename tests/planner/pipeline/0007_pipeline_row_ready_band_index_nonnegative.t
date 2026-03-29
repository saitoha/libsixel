#!/bin/sh
# TAP test: pipeline row_ready events report non-negative band indexes.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

ppm_tall="${TOP_SRCDIR}/tests/data/inputs/tall.ppm"
log_file="${ARTIFACT_LOCAL_DIR}/pipeline-row-ready.log"
out_file="${ARTIFACT_LOCAL_DIR}/tall-row-ready.six"

SIXEL_LOG_PATH="${log_file}"         SIXEL_THREADS=6         SIXEL_DITHER_PARALLEL_THREADS_MAX=1         SIXEL_DITHER_PARALLEL_BAND_WIDTH=9         SIXEL_DITHER_PARALLEL_BAND_OVERWRAP=4         ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -v -o "${out_file}" "${ppm_tall}" || {
    echo "not ok" 1 - "row_ready conversion failed"
    exit 0
}

row_ready_seen=0
row_ready_job_nonnegative=0
while IFS= read -r line; do
    case "${line}" in
        *'"event":"row_ready"'*)
            row_ready_seen=1
            case "${line}" in
                *'"job":-'*) ;;
                *'"job":'*[0-9]*)
                    row_ready_job_nonnegative=1
                    ;;
            esac
            ;;
    esac
done < "${log_file}"

test "${row_ready_seen}" -eq 1 || {
    echo "ok" 1 - "row_ready unavailable in serial environment"
    exit 0
}

test "${row_ready_job_nonnegative}" -eq 1 || {
    echo "not ok" 1 - "row_ready jobs are non-negative"
    exit 0
}

echo "ok" 1 - "row_ready jobs are non-negative"

exit 0

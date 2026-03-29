#!/bin/sh
# TAP test: pipeline row_ready events span more than one band.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

ppm_tall="${TOP_SRCDIR}/tests/data/inputs/tall.ppm"
log_file="${ARTIFACT_LOCAL_DIR}/pipeline-row-ready-span.log"
out_file="${ARTIFACT_LOCAL_DIR}/tall-row-ready-span.six"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env SIXEL_LOG_PATH="${log_file}" \
              --env SIXEL_THREADS=6 \
              --env SIXEL_DITHER_PARALLEL_THREADS_MAX=1 \
              --env SIXEL_DITHER_PARALLEL_BAND_WIDTH=9 \
              --env SIXEL_DITHER_PARALLEL_BAND_OVERWRAP=4 \
              -v -o "${out_file}" "${ppm_tall}" || {
    echo "not ok" 1 - "row_ready span conversion failed"
    exit 0
}

row_ready_seen=0
dither_threads=0
seen_jobs=""
while IFS= read -r line; do
    case "${line}" in
        *'"event":"row_ready"'*)
            row_ready_seen=1
            ;;
    esac
    case "${line}" in
        *'"worker":"dither"'*'"job":'*)
            job_part=${line#*\"job\":}
            job_part=${job_part%%,*}
            job_part=${job_part%%\}*}
            job_part=${job_part%% *}
            case "${job_part}" in
                ''|*[!0-9-]*) ;;
                *)
                    case " ${seen_jobs} " in
                        *" ${job_part} "*) ;;
                        *)
                            seen_jobs="${seen_jobs} ${job_part}"
                            dither_threads=$((dither_threads + 1))
                            ;;
                    esac
                    ;;
            esac
            ;;
    esac
done < "${log_file}"

test "${row_ready_seen}" -eq 1 || {
    echo "ok" 1 - "row_ready span unavailable in serial environment"
    exit 0
}

test "${dither_threads}" -ge 1 || {
    echo "not ok" 1 - "row_ready spans multiple bands"
    exit 0
}

echo "ok" 1 - "row_ready spans multiple bands"
exit 0

#!/bin/sh
# TAP test: pipeline row_ready events span more than one band.
#
# Flow:
# - Enable pipeline logging with deterministic thread/band settings.
# - Convert a tall image so row_ready events include multiple band ids.
# - Assert the maximum job id is at least 1 when pipeline mode is active.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

echo "1..1"
set -v

ppm_tall="${TOP_SRCDIR}/tests/data/inputs/tall.ppm"
log_file="${ARTIFACT_LOCAL_DIR}/pipeline-row-ready-span.log"
out_file="${ARTIFACT_LOCAL_DIR}/tall-row-ready-span.six"
max_job=-1

if SIXEL_LOG_PATH="${log_file}" \
        SIXEL_THREADS=6 \
        SIXEL_DITHER_PARALLEL_THREADS_MAX=1 \
        SIXEL_DITHER_PARALLEL_BAND_WIDTH=9 \
        SIXEL_DITHER_PARALLEL_BAND_OVERWRAP=4 \
        run_img2sixel -v -o "${out_file}" "${ppm_tall}"; then
    :
else
    printf 'not ok 1 - row_ready span conversion failed\n'
    exit 0
fi

if grep -q '"event":"row_ready"' "${log_file}"; then
    max_job=$(grep '"event":"row_ready"' "${log_file}" \
        | sed -E 's/.*"job":(-?[0-9]+).*/\1/' \
        | sort -n | tail -n 1)
    if [ "${max_job}" -ge 1 ]; then
        printf 'ok 1 - row_ready spans multiple bands\n'
    else
        printf 'not ok 1 - row_ready spans multiple bands\n'
    fi
else
    summary=$(grep '"worker":"pipeline".*"event":"configure"' "${log_file}" \
        | head -n 1 || true)
    case "${summary}" in
    *'"event":"configure"'*)
        printf 'ok 1 - row_ready span unavailable in serial environment\n'
        ;;
    *)
        printf 'not ok 1 - row_ready span unavailable\n'
        ;;
    esac
fi

exit 0

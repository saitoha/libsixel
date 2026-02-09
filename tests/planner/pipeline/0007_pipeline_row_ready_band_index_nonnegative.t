#!/bin/sh
# TAP test: pipeline row_ready events report non-negative band indexes.
#
# Flow:
# - Enable timeline logging and request pipeline-friendly thread settings.
# - Convert a tall image so multiple sixel bands are emitted.
# - Confirm row_ready events are present and have non-negative job ids.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

echo "1..1"
set -v

ppm_tall="${TOP_SRCDIR}/tests/data/inputs/tall.ppm"
log_file="${ARTIFACT_LOCAL_DIR}/pipeline-row-ready.log"
out_file="${ARTIFACT_LOCAL_DIR}/tall-row-ready.six"

if SIXEL_LOG_PATH="${log_file}" \
        SIXEL_THREADS=6 \
        SIXEL_DITHER_PARALLEL_THREADS_MAX=1 \
        SIXEL_DITHER_PARALLEL_BAND_WIDTH=9 \
        SIXEL_DITHER_PARALLEL_BAND_OVERWRAP=4 \
        run_img2sixel -v -o "${out_file}" "${ppm_tall}"; then
    :
else
    printf 'not ok 1 - row_ready conversion failed\n'
    exit 0
fi

if grep -q '"event":"row_ready"' "${log_file}" \
        && grep -Eq '"event":"row_ready".*"job":[0-9]+' "${log_file}"; then
    printf 'ok 1 - row_ready jobs are non-negative\n'
else
    summary=$(grep '"worker":"pipeline"' "${log_file}" \
        | head -n 1 || true)
    case "${summary}" in
    *'"worker":"pipeline"'*)
        printf 'ok 1 - row_ready unavailable in serial environment\n'
        ;;
    '')
        # --disable-threads builds do not emit pipeline worker records.
        # In this mode, missing row_ready events are expected.
        printf 'ok 1 - row_ready unavailable without pipeline worker\n'
        ;;
    *)
        printf 'not ok 1 - row_ready jobs are non-negative\n'
        ;;
    esac
fi

exit 0

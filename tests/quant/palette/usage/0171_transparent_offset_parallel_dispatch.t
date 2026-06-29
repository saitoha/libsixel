#!/bin/sh
# Verify transparent-offset still uses the parallel encoder pipeline.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

test "${SIXEL_ENABLE_THREADS-0}" = 1 || {
    printf "1..0 # SKIP thread backend is unavailable\n"
    exit 0
}

echo "1..1"
set -v

artifact_dir=${ARTIFACT_ROOT:-.}
artifact_suffix=$$
input_image="${TOP_SRCDIR}/tests/data/inputs/tall.ppm"
log_file="${artifact_dir}/transparent-offset-parallel-dispatch-${artifact_suffix}.jsonl"
parallel_output="${artifact_dir}/transparent-offset-parallel-${artifact_suffix}.six"
serial_output="${artifact_dir}/transparent-offset-serial-${artifact_suffix}.six"
dispatch_seen=0
serial_fallback_seen=0

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env SIXEL_THREADS=1 \
    --env SIXEL_DITHER_PARALLEL_THREADS_MAX=1 \
    -L builtin -p 16 -+ 3,3 -o "${serial_output}" "${input_image}" || {
    echo "not ok 1 - transparent-offset serial encode failed"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env SIXEL_LOG_PATH="${log_file}" \
    --env SIXEL_THREADS=4 \
    --env SIXEL_DITHER_PARALLEL_THREADS_MAX=1 \
    -L builtin -p 16 -+ 3,3 -o "${parallel_output}" "${input_image}" || {
    echo "not ok 1 - transparent-offset parallel encode failed"
    exit 0
}

cmp -s "${serial_output}" "${parallel_output}" || {
    echo "not ok 1 - transparent-offset parallel output differs"
    exit 0
}

while IFS= read -r line; do
    case "${line}" in
        *'"worker":"encode"'*'"event":"dispatch"'*)
            dispatch_seen=1
            ;;
    esac
    case "${line}" in
        *'serial path threads=1'*)
            serial_fallback_seen=1
            ;;
    esac
done < "${log_file}"

test "${dispatch_seen}" -eq 1 || {
    echo "not ok 1 - transparent-offset did not dispatch encode jobs"
    exit 0
}

test "${serial_fallback_seen}" -eq 0 || {
    echo "not ok 1 - transparent-offset used the serial fallback"
    exit 0
}

echo "ok 1 - transparent-offset uses parallel encode dispatch"
exit 0

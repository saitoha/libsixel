#!/bin/sh
# Verify forced GPU bluenoise default lookup with thread budget 1.
#
# The compatibility root resolves AUTO to the owned exhaustive GPU lookup.
# After a failed conversion, probe the explicit lookup capability so a
# broken AUTO resolver is not mistaken for unavailable hardware.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

# A fixed palette isolates the default GPU lookup from palette construction.
# Probe the explicit lookup only when forced execution fails.
conversion_status=0
probe_status=0
trace=$(set +xv; SIXEL_TRACE_TOPIC=gpu_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        --env SIXEL_ENCODER_THREADING_POLICY=greedy \
        --env SIXEL_ENCODER_THREADING_GLOBAL_BUDGET=1 \
        --env SIXEL_GPU_POLICY=force \
        --env SIXEL_DITHER_BLUENOISE_STRENGTH=0.4 \
        -d bluenoise -b xterm256 -o /dev/null \
        "${TOP_SRCDIR}/tests/data/inputs/snake_64.png" 2>&1) || conversion_status=$?
test "$conversion_status" -eq 0 ||
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" --gpu-lookup-available none || probe_status=$?
test "$probe_status" -ne 77 || {
    printf '\n%s\n' "ok 1 # SKIP GPU direct lookup is unavailable"
    exit 0
}
test "$probe_status" -eq 0 || {
    printf '\n%s\n' "not ok 1 - GPU direct lookup probe failed"
    exit 0
}
test "$conversion_status" -eq 0 || {
    printf '%s\n' "$trace" >&2
    printf '\n%s\n' "not ok 1 - serial forced GPU bluenoise default lookup failed"
    exit 0
}

# Metal reports its direct or GEMM kernel, and Vulkan reports the lookup name.
case "$trace" in
    *'LSXGPU2|consumer=palette-apply|kernel=direct'*|\
    *'LSXGPU2|consumer=palette-apply|kernel=gemm'*|\
    *'LSXGPU2|consumer=palette-apply|kernel=none'*) true ;;
    *) false ;;
esac || {
    printf '%s\n' "$trace" >&2
    printf '\n%s\n' 'not ok 1 - default bluenoise lookup did not use GPU palette application'
    exit 0
}

printf '\n%s\n' "ok 1 - serial forced GPU bluenoise accepts default lookup"
exit 0

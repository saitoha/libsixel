#!/bin/sh
# Verify forced GPU bluenoise default lookup with thread budget 6.
#
# The compatibility root resolves AUTO to the owned exhaustive GPU lookup.
# After a failed conversion, probe the explicit lookup capability so a
# broken AUTO resolver is not mistaken for unavailable hardware.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

input_image="${TOP_SRCDIR}/images/vimperator3.png"

echo "1..1"
set -v

# Successful forced execution proves availability without a separate probe.
conversion_status=0
probe_status=0
${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env SIXEL_ENCODER_THREADING_POLICY=greedy --env SIXEL_ENCODER_THREADING_GLOBAL_BUDGET=6 \
        --env SIXEL_GPU_POLICY=force \
        --env SIXEL_DITHER_BLUENOISE_STRENGTH=0.4 \
        -d bluenoise -p 256 -o /dev/null "${input_image}" || conversion_status=$?
test "$conversion_status" -eq 0 ||
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" --gpu-lookup-available none || probe_status=$?
test "$probe_status" -ne 77 || {
    echo "ok 1 # SKIP GPU direct lookup is unavailable"
    exit 0
}
test "$probe_status" -eq 0 || {
    echo "not ok 1 - GPU direct lookup probe failed"
    exit 0
}
test "$conversion_status" -eq 0 || {
    echo "not ok" 1 - "parallel forced GPU bluenoise default lookup failed"
    exit 0
}

echo "ok 1 - parallel forced GPU bluenoise accepts default lookup"
exit 0

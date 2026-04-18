#!/bin/sh
# TAP test: planner does not insert gradient-map when factor is zero.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

pipeline_log=$(
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        --env SIXEL_DITHER_BLUENOISE_GRADIENT_FACTOR=0 \
        -v -=1 \
        -d bluenoise \
        -o /dev/null \
        "${TOP_SRCDIR}/tests/data/inputs/small.ppm" 2>&1
) || {
    echo "not ok" 1 - "planner run failed for zero gradient factor"
    exit 0
}
printf '%s' "${pipeline_log}" >&2

test "${pipeline_log#*gradient-map*}" = "${pipeline_log}" || {
    echo "not ok" 1 - "planner unexpectedly inserted gradient-map"
    exit 0
}

test "${pipeline_log#*load -> dither*}" != "${pipeline_log}" || {
    echo "not ok" 1 - "planner missed direct load -> dither edge"
    exit 0
}

echo "ok" 1 - "planner skips gradient-map when factor is zero"
exit 0

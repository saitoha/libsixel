#!/bin/sh
# TAP test: planner inserts gradient-map before dither with CLI factor.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

pipeline_log=$(
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        -v -=1 \
        -d bluenoise:G1.0 \
        -o /dev/null \
        "${TOP_SRCDIR}/tests/data/inputs/small.ppm" 2>&1
) || {
    echo "not ok" 1 - "planner run failed for bluenoise CLI gradient_factor"
    exit 0
}
printf '%s' "${pipeline_log}" >&2

test "${pipeline_log#*gradient-map -> dither*}" != "${pipeline_log}" || {
    echo "not ok" 1 - "planner missed gradient-map -> dither edge"
    exit 0
}

echo "ok" 1 - "planner inserts gradient-map before dither for CLI factor"
exit 0

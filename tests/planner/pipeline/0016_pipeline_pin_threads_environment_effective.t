#!/bin/sh
# Verify the effective pipeline pinning policy selected by the environment.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}
test "${SIXEL_ENABLE_THREADS-0}" = 1 || {
    printf "1..0 # SKIP threading is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"

pin_trace=$(set +xv; SIXEL_TRACE_TOPIC=dither_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env "SIXEL_THREADS=6" \
    --env "SIXEL_DITHER_PIN_THREADS=0" -d fs \
    "${input_image}" 2>&1 >/dev/null) || {
    echo "not ok" 1 - "pin threads environment conversion failed"
    exit 0
}

test "${pin_trace#*LSXDTH1|*pin_threads=0*}" != "${pin_trace}" || {
    echo "not ok" 1 - "pin threads was not disabled"
    exit 0
}

invalid_trace=$(set +xv; SIXEL_TRACE_TOPIC=dither_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env "SIXEL_THREADS=6" \
    --env "SIXEL_DITHER_PIN_THREADS=off" -d fs \
    "${input_image}" 2>&1 >/dev/null) || {
    echo "not ok" 1 - "invalid pin threads conversion failed"
    exit 0
}

test "${invalid_trace#*LSXDTH1|*pin_threads=1*}" != \
    "${invalid_trace}" || {
    echo "not ok" 1 - "invalid pin threads changed the default"
    exit 0
}

echo "ok" 1 - "pin threads environment controls affinity"
exit 0

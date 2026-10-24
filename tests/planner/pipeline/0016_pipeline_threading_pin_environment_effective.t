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
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env "SIXEL_THREADING_GLOBAL_BUDGET=6" \
    --env "SIXEL_THREADING_PIN=0" -d fs \
    "${input_image}" 2>&1 >/dev/null) || {
    echo "not ok" 1 - "threading pin environment conversion failed"
    exit 0
}

case "${pin_trace}" in *LSXDTH1\|*pin_threads=0**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "threading pin was not disabled"
    exit 0
}

invalid_trace=$(set +xv; SIXEL_TRACE_TOPIC=dither_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env "SIXEL_THREADING_GLOBAL_BUDGET=6" \
    --env "SIXEL_THREADING_PIN=off" -d fs \
    "${input_image}" 2>&1 >/dev/null) || {
    echo "not ok" 1 - "invalid threading pin conversion failed"
    exit 0
}

case "${invalid_trace}" in *LSXDTH1\|*pin_threads=1**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "invalid threading pin changed the default"
    exit 0
}

echo "ok" 1 - "threading pin environment controls affinity"
exit 0

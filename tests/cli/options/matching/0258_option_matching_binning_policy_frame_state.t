#!/bin/sh
# Verify async palette execution commits binning to per-frame policy state.

set -eux

test "${SIXEL_ENABLE_THREADS-0}" = 1 || {
    printf "1..0 # SKIP thread backend is unavailable\\n"
    exit 0
}

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

trace=$(set +xv; SIXEL_TRACE_TOPIC=palette_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --threads=4 \
    --binning-policy=soft -Qauto -dnone -p16 "-~none" \
    -Lbuiltin -ldisable \
    "${TOP_SRCDIR}/images/snake.png" 2>&1 >/dev/null) || {
    echo "not ok 1 - async binning-policy encode failed"
    exit 0
}

test "${trace#*LSXBPS1|quantizer_requested=0|quantizer_effective=2|quantizer_origin=auto|quantizer_phase=executed|quantizer_reason=quantizer-capability|binning_requested=soft|binning_effective=soft|binning_origin=explicit|binning_phase=executed|binning_reason=explicit|points=*}" != "${trace}" || {
    echo "not ok 1 - async binning state was not committed"
    exit 0
}

test "${trace#*LSXSPL1|requested=auto|effective=adaptive-grid|source=loaded-frame|origin=auto|phase=executed|reason=resource-profile|threads=4|heavy=0|budget_async=1|job_ready=1*}" != "${trace}" || {
    echo "not ok 1 - palette work did not use the asynchronous path"
    exit 0
}

test "${trace#*LSXPFB1*}" = "${trace}" || {
    echo "not ok 1 - asynchronous palette work used a fallback"
    exit 0
}

echo "ok 1 - async binning result is retained in per-frame state"
exit 0

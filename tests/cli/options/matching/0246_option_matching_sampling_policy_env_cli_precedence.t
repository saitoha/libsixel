#!/bin/sh
# Verify explicit sampling-policy keeps command-line priority over environment.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

trace=$(set +xv; SIXEL_TRACE_TOPIC=palette_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --threads=1 \
    --env SIXEL_SAMPLING_POLICY=full-frame \
    --sampling-policy=adaptive-grid \
    -Qheckbert -d none -p 16 "-~none" -L builtin -ldisable \
    "${TOP_SRCDIR}/images/snake.png" 2>&1 >/dev/null) || {
    echo "not ok 1 - sampling-policy precedence encode failed"
    exit 0
}

test "${trace#*LSXSPL1|requested=adaptive-grid|effective=adaptive-grid|source=loaded-frame|origin=explicit|phase=executed|reason=explicit*}" != "${trace}" || {
    echo "not ok 1 - environment overrode command-line sampling-policy"
    exit 0
}

echo "ok 1 - command-line sampling-policy overrides environment"
exit 0

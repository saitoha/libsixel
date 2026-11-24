#!/bin/sh
# Verify bypassed sampling leaves both threads available to loader handoff.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\\n"
    exit 0
}

test "${SIXEL_ENABLE_THREADS-0}" = 1 || {
    printf "1..0 # SKIP thread backend is unavailable\\n"
    exit 0
}

echo "1..1"
set -v

trace=$(set +xv; SIXEL_TRACE_TOPIC=encode_handoff \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --threading-policy=greedy:global_budget=2 \
    -b xterm16 -d none "-~none" -L builtin -ldisable -g \
    "${TOP_SRCDIR}/tests/data/inputs/formats/apng_8x8_rgba_loop2.png" \
    -o/dev/null 2>&1) || {
    printf '\n%s\n' "not ok 1 - built-in palette handoff encode failed"
    exit 0
}

case "${trace}" in *event=callback_handoff_decide\ handoff=pipeline**) true ;; *) false ;; esac || {
    printf '\n%s\n' "not ok 1 - bypassed sampling reserved a palette worker"
    exit 0
}

printf '\n%s\n' "ok 1 - bypassed sampling leaves two threads for handoff"
exit 0

#!/bin/sh
# Verify generated RGB loader sessions and sequence continuity across loops.
set -eux

test "${HAVE_IMG2SIXEL-0}:${SIXEL_ENABLE_THREADS-0}" = 1:1 || {
    echo "1..0 # SKIP threaded converter is disabled"
    exit 0
}

echo "1..1"
set -v

trace=$(set +xv; SIXEL_TRACE_TOPIC=palette_latest \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -g -Lbuiltin! \
    -=greedy:B4:palette=latest:load=ahead:prepare=ahead:load_depth=4 \
    -4 full-frame -o /dev/null \
    "${TOP_SRCDIR}/tests/data/inputs/formats/apng_8x8_rgb_loop2.png" 2>&1) || {
    echo "not ok 1 - latest RGB loader execution failed"
    exit 0
}
case "${trace}" in
    *'LSXLATEST1|frame=1|palette=1|'*'LSXLATEST1|frame=2|'*'LSXLATEST1|frame=3|'*'LSXLATEST1|frame=4|'*) true ;;
    *) false ;;
esac || {
    echo "not ok 1 - RGB loader did not preserve session sequence across loops"
    exit 0
}
echo "ok 1 - latest loader ahead/ahead loop sequence"
exit 0

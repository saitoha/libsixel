#!/bin/sh
# Verify compact, long and environment routes reach the AUTO planner.
# Policy: docs/cli/threading-policy.md
set -eux

test "${HAVE_IMG2SIXEL-0}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled"
    exit 0
}
echo "1..1"
set -v

trace=$(set +xv; SIXEL_ENCODER_THREADING_PALETTE=auto \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -v '-=greedy:B1:Cauto' \
    -Lbuiltin! -w16 -h16 "${TOP_SRCDIR}/images/snake.png" \
    2>&1 >/dev/null) || {
    echo "not ok 1 - auto route conversion failed"
    exit 0
}
case "${trace}" in
    *'palette=auto palette_effective=frame'*) true ;;
    *) false ;;
esac || {
    echo "not ok 1 - auto route did not reach planner"
    exit 0
}
trace=$(set +xv; SIXEL_ENCODER_THREADING_PALETTE=auto \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -v '-=greedy:B1:palette=auto' \
    -Lbuiltin! -w16 -h16 "${TOP_SRCDIR}/images/snake.png" \
    2>&1 >/dev/null) || {
    echo "not ok 1 - auto route conversion failed"
    exit 0
}
case "${trace}" in
    *'palette=auto palette_effective=frame'*) true ;;
    *) false ;;
esac || {
    echo "not ok 1 - auto route did not reach planner"
    exit 0
}
trace=$(set +xv; SIXEL_ENCODER_THREADING_PALETTE=auto \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -v '-=greedy:B1' \
    -Lbuiltin! -w16 -h16 "${TOP_SRCDIR}/images/snake.png" \
    2>&1 >/dev/null) || {
    echo "not ok 1 - auto route conversion failed"
    exit 0
}
case "${trace}" in
    *'palette=auto palette_effective=frame'*) true ;;
    *) false ;;
esac || {
    echo "not ok 1 - auto route did not reach planner"
    exit 0
}
trace=$(set +xv; SIXEL_ENCODER_THREADING_PALETTE=auto \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -v '-=greedy:B1:palette=frame' \
    -Lbuiltin! -w16 -h16 "${TOP_SRCDIR}/images/snake.png" \
    2>&1 >/dev/null) || {
    echo "not ok 1 - frame override failed"
    exit 0
}
case "${trace}" in
    *'palette=frame palette_effective=frame'*) true ;;
    *) false ;;
esac || {
    echo "not ok 1 - explicit frame did not override auto environment"
    exit 0
}
echo "ok 1 - auto option routes and explicit frame precedence"
exit 0

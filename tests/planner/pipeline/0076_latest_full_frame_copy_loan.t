#!/bin/sh
# Policy: docs/cli/threading-policy.md
# Verify the latest-only owned input copy uses a loan in a real session.
set -eux

test "${SIXEL_ENABLE_THREADS-0}" = 1 || {
    echo "1..0 # SKIP threading is disabled"
    exit 0
}
test "${HAVE_IMG2SIXEL-0}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled"
    exit 0
}

echo "1..1"
set -v

trace=$(set +xv; SIXEL_TRACE_TOPIC=palette_latest \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -g -v -l disable -Lbuiltin! \
    "-=greedy:B4:palette=latest:prepare=ahead" -4 full-frame -p16 \
    -o /dev/null \
    "${TOP_SRCDIR}/tests/data/inputs/snake_motion_64_2frame.gif" 2>&1) || {
    echo "not ok 1 - latest full-frame conversion failed"
    echo "${trace}"
    exit 0
}
case "${trace}" in
    *'LSXLOAN1|event=background|worker=2|begin=1|controller=0|'*) true ;;
    *) false ;;
esac || {
    echo "not ok 1 - latest input copy did not borrow an idle slot"
    echo "${trace}"
    exit 0
}
case "${trace}" in
    *'emit=4'*'LSXLATEST1|frame=2|'*) true ;;
    *) false ;;
esac || {
    echo "not ok 1 - latest session did not retain full emission allocation"
    echo "${trace}"
    exit 0
}
echo "ok 1 - latest full-frame input copy uses an idle slot"
exit 0

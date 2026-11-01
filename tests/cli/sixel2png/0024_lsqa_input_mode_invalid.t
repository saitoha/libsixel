#!/bin/sh
# Policy: docs/decoder/multi-pass.md
set -eux

test "${HAVE_SIXEL2PNG-}" = 1 || {
    echo "1..0 # SKIP sixel2png is disabled"
    exit 0
}
test -n "${LSQA_PATH-}" && test -x "${LSQA_PATH}" || {
    echo "1..0 # SKIP lsqa is disabled"
    exit 0
}

echo "1..1"
set -v

status=0
${SIXEL_RUNTIME-} "${LSQA_PATH}" -M "" \
    "${TOP_SRCDIR}/tests/data/inputs/snake_16.png" \
    "${TOP_SRCDIR}/tests/data/inputs/progressive-snake-16.six" >/dev/null 2>&1 || status=$?
test "${status}" -eq 2 || {
    echo "not ok 1 - invalid input mode was not rejected: "
    exit 0
}

status=0
${SIXEL_RUNTIME-} "${LSQA_PATH}" -M "Single" \
    "${TOP_SRCDIR}/tests/data/inputs/snake_16.png" \
    "${TOP_SRCDIR}/tests/data/inputs/progressive-snake-16.six" >/dev/null 2>&1 || status=$?
test "${status}" -eq 2 || {
    echo "not ok 1 - invalid input mode was not rejected: Single"
    exit 0
}

status=0
${SIXEL_RUNTIME-} "${LSQA_PATH}" -M "6delta-progressive-extra" \
    "${TOP_SRCDIR}/tests/data/inputs/snake_16.png" \
    "${TOP_SRCDIR}/tests/data/inputs/progressive-snake-16.six" >/dev/null 2>&1 || status=$?
test "${status}" -eq 2 || {
    echo "not ok 1 - invalid input mode was not rejected: 6delta-progressive-extra"
    exit 0
}

echo "ok 1 - invalid input modes remain usage errors"
exit 0

#!/bin/sh
# Verify malformed final input produces no prefix PNG.
# Policy: docs/decoder/multi-pass.md

set -eux

test "${HAVE_SIXEL2PNG-}" = 1 || {
    echo "1..0 # SKIP sixel2png is disabled"
    exit 0
}

echo "1..1"
set -v

output=$(printf '\033P0;1q"1;1;1;1#0;2;100;0;0@\033\\\033P' | \
    ${SIXEL_RUNTIME-} "${SIXEL2PNG_PATH}" -M 6delta-progressive \
    2>/dev/null) && {
    echo "not ok 1 - incomplete suffix accepted"
    exit 0
}
test -z "${output}" || {
    echo "not ok 1 - prefix PNG published before EOF validation"
    exit 0
}

echo "ok 1 - multi pass late error"
exit 0

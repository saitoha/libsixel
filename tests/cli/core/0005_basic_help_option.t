#!/bin/sh
# TAP test verifying sixel2png help command remains callable.

set -eux

test "${HAVE_SIXEL2PNG-}" = 1 || {
    printf "1..0 # SKIP sixel2png is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v
status=0

set +x
${SIXEL_RUNTIME-} "${SIXEL2PNG_PATH}" -H >/dev/null 2>&1 || status=$?
set -x

test "${status}" -eq 0 || {
    echo "not ok" 1 - "sixel2png help option failed"
    exit 0
}

echo "ok" 1 - "sixel2png help command remains callable"
exit 0

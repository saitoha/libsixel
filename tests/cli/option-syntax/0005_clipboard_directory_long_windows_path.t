#!/bin/sh
# Verify long clipboard directory syntax keeps a Windows drive separator.

set -eux

test "${HAVE_SIXEL2PNG-}" = 1 || {
    echo "1..0 # SKIP sixel2png is disabled in this build"
    exit 0
}

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${SIXEL2PNG_PATH}" \
    -y'file:directory=C:/clipboard-test' -H >/dev/null || {
    printf '\n%s\n' "not ok 1 - clipboard directory long syntax split its drive"
    exit 0
}

printf '\n%s\n' "ok 1 - clipboard directory long syntax preserves Windows paths"
exit 0

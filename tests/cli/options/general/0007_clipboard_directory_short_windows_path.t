#!/bin/sh
# Verify compact clipboard directory syntax keeps a Windows drive separator.

set -eux

test "${HAVE_SIXEL2PNG-}" = 1 || {
    echo "1..0 # SKIP sixel2png is disabled in this build"
    exit 0
}

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${SIXEL2PNG_PATH}" \
    -y'file:DC:/clipboard-test' -H >/dev/null || {
    echo "not ok" 1 - "clipboard directory short syntax split its drive"
    exit 0
}

echo "ok" 1 - "clipboard directory short syntax preserves Windows paths"
exit 0

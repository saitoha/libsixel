#!/bin/sh
# TAP test verifying img2sixel help command remains callable.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

status=0

set +x
${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --help >/dev/null 2>&1 || status=$?
set -x

test "${status}" -eq 0 || {
    echo "not ok" 1 - "failed to load runtime help from img2sixel"
    exit 0
}

echo "ok" 1 - "img2sixel help command remains callable"
exit 0

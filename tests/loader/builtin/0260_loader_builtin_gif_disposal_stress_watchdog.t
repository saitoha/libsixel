#!/bin/sh
# TAP test: disposal-heavy GIF decode completes within watchdog window.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}


printf '1..1\n'
set -v

input_gif="${TOP_SRCDIR}/tests/data/inputs/formats/gif-disposal-stress-anim.gif"
command_status=0

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Lbuiltin! -ldisable -g "${input_gif}" \
    -o /dev/null >/dev/null 2>&1 || command_status=$?
test "${command_status}" -eq 0 || {
    echo "not ok" 1 - "disposal stress decode failed"
    exit 0
}

echo "ok" 1 - "disposal stress decode completed within watchdog"
exit 0

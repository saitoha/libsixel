#!/bin/sh
# TAP test: unknown NETSCAPE loop subtype does not cause force-loop hang.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}


echo "1..1"
set -v

input_gif="${TOP_SRCDIR}/tests/data/inputs/formats/gif-anim-netscape-unknown-subtype.gif"
command_status=0

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env SIXEL_THREADS=4 -Lbuiltin! \
    -lforce -d none:scan=raster -p 256 "${input_gif}" >/dev/null 2>/dev/null \
    || command_status=$?
test "${command_status}" -eq 0 || {
    echo "not ok" 1 - "builtin unknown NETSCAPE subtype decode failed"
    exit 0
}

echo "ok" 1 - "builtin unknown NETSCAPE subtype does not hang in force-loop"
exit 0

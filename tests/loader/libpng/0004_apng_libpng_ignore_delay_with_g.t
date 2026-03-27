#!/bin/sh
# TAP test: APNG ignores frame delay with -g.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


test "${HAVE_LIBPNG-}" = 1 || {
    printf "1..0 # SKIP libpng is disabled in this build\n";
    exit 0
}

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env SIXEL_TRACE_TOPIC=encode_handoff,apng_decode,lifecycle -Llibpng! -g "${TOP_SRCDIR}/tests/data/inputs/formats/apng_8x8_rgba_loop2.png" -o/dev/null || {
    echo "not ok" 1 - "APNG ignore-delay mode failed"
    exit 0
}

echo "ok" 1 - "APNG ignore-delay mode succeeds"
exit 0


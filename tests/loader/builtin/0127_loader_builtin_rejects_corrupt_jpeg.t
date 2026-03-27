#!/bin/sh
# TAP test confirming builtin loader rejects corrupted JPEG streams.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}


echo "1..1"
set -v

input_jpeg="${TOP_SRCDIR}/tests/data/corrupted/metadata_noise.jpg"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L builtin! "${input_jpeg}" >/dev/null && {
    echo "not ok" 1 - "corrupted JPEG unexpectedly decoded by builtin"
    exit 0
}

echo "ok" 1 - "corrupted JPEG is rejected by builtin"
exit 0

#!/bin/sh
# TAP test confirming the builtin loader can decode PSD inputs.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v

input_psd="${TOP_SRCDIR}/tests/data/inputs/formats/stbi_minimal.psd"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" "${input_psd}" >/dev/null || {
    echo "not ok" 1 - "builtin loader failed to decode PSD"
    exit 0
}

echo "ok" 1 - "builtin loader decodes PSD"
exit 0

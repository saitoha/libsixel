#!/bin/sh
# TAP test: quicklook does not hang on invalid-signature HEIF input.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}


test "${HAVE_QUICKLOOK-}" = 1 || {
    printf "1..0 # SKIP quicklook loader is unavailable\n"
    exit 0
}

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env SIXEL_THUMBNAILER_HINT_SIZE=64 -L quicklook! \
    "${TOP_SRCDIR}/tests/data/corrupted/invalid_signature.heif" >/dev/null 2>/dev/null || :

echo "ok" 1 - "quicklook does not hang on invalid-signature HEIF"
exit 0

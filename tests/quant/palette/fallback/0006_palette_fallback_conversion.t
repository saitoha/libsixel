#!/bin/sh
# TAP test: PAL1 input expands via fallback path when tables are disabled.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build";
    exit 0
}

echo "1..1"
set -v

snake_ascii_pbm="${TOP_SRCDIR}/images/snake-ascii.pbm"

run_img2sixel --env SIXEL_PALETTE_DISABLE_TABLES=1 "${snake_ascii_pbm}" -o "${ARTIFACT_LOCAL_DIR}/snake-fallback.sixel" || {
    printf 'not ok 1 - fallback expansion failed\n'
    exit 0
}

printf 'ok 1 - PAL1 input expands via fallback path\n'
exit 0

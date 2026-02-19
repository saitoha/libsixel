#!/bin/sh
# TAP test ensuring img2sixel rejects missing palette files.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

echo "1..1"
set -v

run_img2sixel -m "${ARTIFACT_LOCAL_DIR}/invalid_filename" \
    "${TOP_SRCDIR}/tests/data/inputs/snake_64.jpg" </dev/null >/dev/null  && {
    fail 1 "unexpected success: missing palette file"
    exit 0
}

pass 1 "invalid option rejected"
exit 0

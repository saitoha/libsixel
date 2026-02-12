#!/bin/sh
# TAP test ensuring img2sixel rejects missing palette files.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

config_macro_defined HAVE_IMG2SIXEL || skip_all "img2sixel is disabled in this build"

echo "1..1"
set -v

run_img2sixel -m "${ARTIFACT_LOCAL_DIR}/invalid_filename" \
    "${TOP_SRCDIR}/tests/data/inputs/snake_64.jpg" </dev/null >/dev/null  && {
    fail 1 "unexpected success: missing palette file"
    exit 0
}

pass 1 "invalid option rejected"
exit 0

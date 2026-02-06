#!/bin/sh
# TAP test ensuring img2sixel rejects missing palette file.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

config_macro_defined HAVE_IMG2SIXEL || skip_all "img2sixel is disabled in this build"

echo "1..1"
set -v

if run_img2sixel -m "${ARTIFACT_LOCAL_DIR}/invalid_filename" \
    "${TOP_SRCDIR}/tests/data/inputs/snake_64.jpg" >/dev/null; then
    fail 1 "unexpected success: missing palette file"
    exit 0
fi

pass 1 "invalid option rejected"
exit 0

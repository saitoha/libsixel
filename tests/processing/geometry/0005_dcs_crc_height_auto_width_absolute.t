#!/bin/sh
# Validate DCS coordinates when width is absolute and height auto.
set -eux

conversion_common_path="${TOP_SRCDIR}/tests/lib/sh/conversion/common.sh"
. "${conversion_common_path}"

status=0

ensure_img2sixel_available

echo "1..1"
set -v

check_dcs_crc 1 "-hauto -w12" \
    "automatic height with width scaling stays consistent"

exit "${status}"

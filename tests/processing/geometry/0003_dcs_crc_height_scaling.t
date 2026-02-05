#!/bin/sh
# Check DCS coordinates when scaling height.
set -eux

conversion_common_path="${TOP_SRCDIR}/tests/lib/sh/conversion/common.sh"
. "${conversion_common_path}"

status=0

ensure_img2sixel_available

echo "1..1"
set -v

check_dcs_crc 1 "-h200%" "height scaling preserves DCS coordinates"

exit "${status}"

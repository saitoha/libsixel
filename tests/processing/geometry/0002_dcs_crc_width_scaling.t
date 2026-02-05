#!/bin/sh
# Check DCS coordinates when scaling width.
set -eux

conversion_common_path="${TOP_SRCDIR}/tests/lib/sh/conversion/common.sh"
. "${conversion_common_path}"

status=0

ensure_img2sixel_available

echo "1..1"
set -v

check_dcs_crc 1 "-w200%" "width scaling preserves DCS coordinates"

exit "${status}"

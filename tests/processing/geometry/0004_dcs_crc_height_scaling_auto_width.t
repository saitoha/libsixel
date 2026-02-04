#!/bin/sh
# Validate DCS coordinates when height scaled and width auto.
set -eux

conversion_common_path="${TOP_SRCDIR}/tests/lib/sh/conversion/common.sh"
. "${conversion_common_path}"

test_name=$(basename "$0")
setup_conversion_env "${test_name}"

status=0

ensure_img2sixel_available

echo "1..1"
set -v

check_dcs_crc 1 "-h200% -wauto" \
    "automatic width with height scaling stays consistent"

exit "${status}"

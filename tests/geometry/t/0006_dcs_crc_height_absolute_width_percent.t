#!/bin/sh
# Validate DCS coordinates when combining absolute height and percent width.
set -eux

conversion_common_path=$(CDPATH=; cd "$(dirname "$0")/.." && pwd)/../conversion/common.sh
. "${conversion_common_path}"

test_name=$(basename "$0")
setup_conversion_env "${test_name}"

status=0

ensure_img2sixel_available

echo "1..1"

check_dcs_crc 1 "-h12 -w200%" \
    "combined absolute and percentage scaling consistent"

exit "${status}"

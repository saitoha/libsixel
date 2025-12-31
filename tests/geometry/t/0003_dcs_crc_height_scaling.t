#!/bin/sh
# Check DCS coordinates when scaling height.
set -eux

conversion_common_path=$(CDPATH=; cd "$(dirname "$0")/.." && pwd)/../conversion/common.sh
. "${conversion_common_path}"

test_name=$(basename "$0")
setup_conversion_env "${test_name}"

status=0

ensure_img2sixel_available

echo "1..1"

check_dcs_crc 1 "-h200%" "height scaling preserves DCS coordinates"

exit "${status}"

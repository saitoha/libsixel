#!/bin/sh
# Check DCS coordinates when scaling width.
set -eux

conversion_common_path=$(CDPATH=; cd "$(dirname "$0")/.." && pwd)/../lib/sh/conversion/common.sh
. "${conversion_common_path}"

test_name=$(basename "$0")
setup_conversion_env "${test_name}"

status=0

ensure_img2sixel_available

echo "1..1"

check_dcs_crc 1 "-w200%" "width scaling preserves DCS coordinates"

exit "${status}"

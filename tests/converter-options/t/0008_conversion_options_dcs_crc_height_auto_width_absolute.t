#!/bin/sh
# Validate DCS coordinates when width is absolute and height auto.
set -eux

. "$(CDPATH=; cd "$(dirname "$0")" && pwd)/converter_options_common.sh"

test_name=$(basename "$0")
setup_converter_options_env "${test_name}"

status=0

ensure_img2sixel_available

echo "1..1"

converter_check_dcs_crc 1 "-hauto -w12" \
    "automatic height with width scaling stays consistent"

exit "${status}"

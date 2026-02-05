#!/bin/sh
# Verify long option forms are accepted.
set -eux

conversion_common_path="${TOP_SRCDIR}/tests/lib/sh/conversion/common.sh"
. "${conversion_common_path}"

setup_conversion_env "${test_name}"

status=0

ensure_img2sixel_available

echo "1..1"
set -v

snake_jpg="${TOP_SRCDIR}/tests/data/inputs/snake_64.jpg"
longopt_sixel="${ARTIFACT_LOCAL_DIR}/snake-longopt.sixel"

if run_img2sixel --height=100 --diffusion=atkinson \
    --outfile="${longopt_sixel}" <"${snake_jpg}"; then
    pass 1 "long option forms accepted"
else
    fail 1 "long option forms failed"
fi

exit "${status}"

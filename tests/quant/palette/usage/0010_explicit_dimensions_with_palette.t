#!/bin/sh
# Check explicit dimensions and palette options work together.
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"
. "${TOP_SRCDIR}/tests/lib/sh/common/tap.sh"

status=0

config_macro_defined HAVE_IMG2SIXEL || skip_all "img2sixel is disabled in this build"

echo "1..1"
set -v

snake_jpg="${TOP_SRCDIR}/tests/data/inputs/snake_64.jpg"
snake_dims="${ARTIFACT_LOCAL_DIR}/snake-dims.sixel"

if run_img2sixel -w210 -h210 -djajuni -bxterm256 -o "${snake_dims}" <"${snake_jpg}"; then
    pass 1 "explicit dimensions and palette options succeeded"
else
    fail 1 "explicit dimensions and palette options failed"
fi

exit "${status}"

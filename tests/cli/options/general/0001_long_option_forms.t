#!/bin/sh
# Verify long option forms are accepted.
set -eux

. "${TOP_SRCDIR}/tests/lib/sh/common/tap.sh"
. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

status=0

config_macro_defined HAVE_IMG2SIXEL || skip_all "img2sixel is disabled in this build"

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

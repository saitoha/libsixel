#!/bin/sh
# Verify long option forms are accepted.
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${HAVE_IMG2SIXEL-}" = 1 || skip_all "img2sixel is disabled in this build"

echo "1..1"
set -v

snake_jpg="${TOP_SRCDIR}/tests/data/inputs/snake_64.jpg"
longopt_sixel="${ARTIFACT_LOCAL_DIR}/snake-longopt.sixel"

run_img2sixel --height=100 --diffusion=atkinson     --outfile="${longopt_sixel}" <"${snake_jpg}" || {
    fail 1 "long option forms rejected"
    exit 0
}

pass 1 "long option forms accepted"
exit 0

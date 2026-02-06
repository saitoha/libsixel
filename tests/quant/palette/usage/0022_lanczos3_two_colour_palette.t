#!/bin/sh
# Scale with Lanczos3 filter using a two-colour palette.
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"
. "${TOP_SRCDIR}/tests/lib/sh/common/tap.sh"

status=0

config_macro_defined HAVE_IMG2SIXEL || skip_all "img2sixel is disabled in this build"

echo "1..1"
set -v

snake_jpg="${TOP_SRCDIR}/tests/data/inputs/snake_64.jpg"
target_sixel="${ARTIFACT_LOCAL_DIR}/lanczos3-two-colour.sixel"

if run_img2sixel -p 2 -h100 -wauto -rlanczos3 "${snake_jpg}" \
        >"${target_sixel}"; then
    pass 1 "Lanczos3 scaling with two-colour palette works"
else
    fail 1 "Lanczos3 scaling with two-colour palette fails"
fi

exit "${status}"

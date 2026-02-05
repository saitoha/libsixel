#!/bin/sh
# Scale with Lanczos3 filter using a two-colour palette.
set -eux

conversion_common_path="${TOP_SRCDIR}/tests/lib/sh/conversion/common.sh"
. "${conversion_common_path}"

status=0

ensure_img2sixel_available

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

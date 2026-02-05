#!/bin/sh
# Scale with Lanczos4 filter and emit palette dump.
set -eux

conversion_common_path="${TOP_SRCDIR}/tests/lib/sh/conversion/common.sh"
. "${conversion_common_path}"

setup_conversion_env "${test_name}"

status=0

ensure_img2sixel_available

echo "1..1"
set -v

snake_jpg="${TOP_SRCDIR}/tests/data/inputs/snake_64.jpg"
target_sixel="${ARTIFACT_LOCAL_DIR}/lanczos4-palette-dump.sixel"

if run_img2sixel -e -h140 -rlanczos4 -P "${snake_jpg}" \
        >"${target_sixel}"; then
    pass 1 "Lanczos4 scaling emits palette dump"
else
    fail 1 "Lanczos4 scaling palette dump fails"
fi

exit "${status}"

#!/bin/sh
# Perform two-pass Sixel conversion to validate re-encoding path.
set -eux

conversion_common_path="${TOP_SRCDIR}/tests/lib/sh/conversion/common.sh"
. "${conversion_common_path}"

status=0

ensure_img2sixel_available

echo "1..1"
set -v

snake_png="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
stage1="${ARTIFACT_LOCAL_DIR}/two-pass-stage1.sixel"
stage2="${ARTIFACT_LOCAL_DIR}/two-pass-stage2.sixel"

if ! run_img2sixel -w204 -h204 "${snake_png}" >"${stage1}"; then
    fail 1 "two-pass Sixel conversion fails"
elif ! run_img2sixel <"${stage1}" >"${stage2}"; then
    fail 1 "two-pass Sixel conversion fails"
else
    pass 1 "two-pass Sixel conversion succeeds"
fi

exit "${status}"

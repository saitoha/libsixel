#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/hdr.md
# Verify hdr xyze signed rle against independent XYZ matrix values.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    --env SIXEL_LOADER_HDR_EXPOSURE_EV=0 \
    --env SIXEL_LOADER_HDR_USE_HEADER_EXPOSURE=1 \
    --env SIXEL_LOADER_HDR_TONEMAP=none \
    loader/hdr/2259_hdr_xyze_signed_rle 1>&2 || {
    echo "not ok 1 - hdr xyze signed rle"
    exit 0
}
echo "ok 1 - hdr xyze signed rle"
exit 0

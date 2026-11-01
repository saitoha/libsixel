#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/hdr.md
# Verify hdr xyze signed colorcorr against independent XYZ matrix values.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    --env SIXEL_LOADER_HDR_EXPOSURE_EV=0 \
    --env SIXEL_LOADER_HDR_USE_HEADER_EXPOSURE=1 \
    --env SIXEL_LOADER_HDR_TONEMAP=none \
    loader/hdr/2261_hdr_xyze_signed_colorcorr 1>&2 || {
    echo "not ok 1 - hdr xyze signed colorcorr"
    exit 0
}
echo "ok 1 - hdr xyze signed colorcorr"
exit 0

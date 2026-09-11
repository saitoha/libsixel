#!/bin/sh
# Policy: docs/loader/builtin-cms.md
# Verify D2B0/B2D0 routing with legacy mft2 payloads.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "icc/0008_icc_builtin_d2b_b2d_legacy_lut" 1>&2 || {
    echo "not ok" 1 - "icc builtin D2B0/B2D0 legacy LUT routing"
    exit 0
}

echo "ok" 1 - "icc builtin D2B0/B2D0 legacy LUT routing"
exit 0

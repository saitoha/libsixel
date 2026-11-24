#!/bin/sh
# Verify K-center canonicalizes packed alpha layouts before shared binning.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "palette/0044_kcenter_packed_alpha_layouts" || {
    printf '\n%s\n' "not ok 1 - 0044_kcenter_packed_alpha_layouts"
    exit 0
}

printf '\n%s\n' "ok 1 - 0044_kcenter_packed_alpha_layouts"
exit 0

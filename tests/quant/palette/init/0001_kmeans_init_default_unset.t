#!/bin/sh
# TAP test: k-means init output check.

set -eux


echo "1..1"
set -v

output=$(
   ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" --env SIXEL_PALETTE_KMEANS_INITTYPE= "palette/0001_kmeans_init" | tr -d '\r'
) || output=""

test "${output}" = "none" || {
    echo "not ok" 1 - "unexpected kmeans init output: ${output}"
    exit 0
}

echo "ok" 1 - "kmeans init output matched: none"

exit 0

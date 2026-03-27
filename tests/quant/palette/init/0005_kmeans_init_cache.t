#!/bin/sh
# TAP test: cached k-means init type preserves PCA selection.

set -eux


echo "1..1"
set -v

output=$(
   ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" --env SIXEL_PALETTE_KMEANS_INITTYPE=pca "palette/0001_kmeans_init" --cache | tr -d '\r'
)

test "${output}" = "pca pca" || {
    echo "not ok" 1 - "cache check returned ${output}"
    exit 0
}

echo "ok" 1 - "cache preserves initial value"
exit 0

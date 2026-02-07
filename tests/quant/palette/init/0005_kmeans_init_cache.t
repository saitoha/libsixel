#!/bin/sh
# TAP test: cached k-means init type preserves PCA selection.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v

output=$(
   run_test_runner --env SIXEL_PALETTE_KMEANS_INITTYPE=pca "palette/0001_kmeans_init" --cache | tr -d '\r'
) || status=$?

if [ "${output}" = "pca pca" ]; then
    printf 'ok 1 - cache preserves initial value\n'
else
    printf 'not ok 1 - cache check returned %s\n' "${output}"
    exit 1
fi

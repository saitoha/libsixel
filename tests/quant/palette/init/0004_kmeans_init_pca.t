#!/bin/sh
# TAP test: PCA k-means init type is honored.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v

output=$(
   run_test_runner --env SIXEL_PALETTE_KMEANS_INITTYPE=pca "palette/0001_kmeans_init" | tr -d '\r'
) || status=$?

if [ "${output}" = "pca" ]; then
    printf 'ok 1 - pca init type accepted\n'
else
    printf 'not ok 1 - pca init type returned %s\n' "${output}"
    exit 1
fi

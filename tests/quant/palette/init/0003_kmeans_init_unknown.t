#!/bin/sh
# TAP test: k-means init output check.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v

output=$(
   run_test_runner --env SIXEL_PALETTE_KMEANS_INITTYPE="unknown" "palette/0001_kmeans_init" | tr -d ''
) || output=""

[ "${output}" = "none" ] || {
    fail 1 "unexpected kmeans init output: ${output}"
    exit 0
}

pass 1 "kmeans init output matched: none"

exit 0

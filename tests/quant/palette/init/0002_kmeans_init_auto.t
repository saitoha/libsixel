#!/bin/sh
# TAP test: auto k-means init type falls back to none.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v

output=$(
   SIXEL_PALETTE_KMEANS_INITTYPE=auto run_test_runner "palette/0001_kmeans_init" | tr -d '\r'
) || status=$

if [ "${output}" = "none" ]; then
    printf 'ok 1 - auto falls back to none\n'
else
    printf 'not ok 1 - auto produced %s\n' "${output}"
    exit 1
fi

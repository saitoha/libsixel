#!/bin/sh
# TAP test: cached k-means init type preserves PCA selection.

set -eux



script_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
. "${TOP_SRCDIR}/tests/lib/sh/palette/kmeans_init_common.sh"

echo "1..1"
set -v

cache_line=$(run_kmeans_init "pca" --cache)
if [ "${cache_line}" = "pca pca" ]; then
    printf 'ok 1 - cache preserves initial value\n'
else
    printf 'not ok 1 - cache check returned %s\n' "${cache_line}"
    exit 1
fi

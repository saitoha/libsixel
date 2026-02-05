#!/bin/sh
# TAP test: PCA k-means init type is honored.

set -eux



script_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
. "${TOP_SRCDIR}/tests/lib/sh/palette/kmeans_init_common.sh"

echo "1..1"
set -v

output=$(run_kmeans_init "pca")
if [ "${output}" = "pca" ]; then
    printf 'ok 1 - pca init type accepted\n'
else
    printf 'not ok 1 - pca init type returned %s\n' "${output}"
    exit 1
fi

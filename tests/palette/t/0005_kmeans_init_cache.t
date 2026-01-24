#!/bin/sh
# TAP test: cached k-means init type preserves PCA selection.

set -euxv

test_name=$(basename "$0")
test_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
category_name=$(basename "$(dirname "${test_dir}")")
artifact_root=${ARTIFACT_ROOT:-"$(pwd)/_artifacts"}
artifact_dir="${artifact_root}/${category_name}/${test_name}"

mkdir -p "${artifact_dir}"

script_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
. "${script_dir}/../../lib/sh/palette/kmeans_init_common.sh"

echo "1..1"

cache_line=$(run_kmeans_init "pca" --cache)
if [ "${cache_line}" = "pca pca" ]; then
    printf 'ok 1 - cache preserves initial value\n'
else
    printf 'not ok 1 - cache check returned %s\n' "${cache_line}"
    exit 1
fi
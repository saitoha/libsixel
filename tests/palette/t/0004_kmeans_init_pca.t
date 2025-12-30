#!/bin/sh
# TAP test: PCA k-means init type is honored.

set -euxv

test_name=$(basename "$0")
artifact_root=${ARTIFACT_ROOT:-"$(pwd)/_artifacts"}
artifact_dir="${artifact_root}/${test_name}"

mkdir -p "${artifact_dir}"

script_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
. "${script_dir}/kmeans_init_common.sh"

echo "1..1"

output=$(run_kmeans_init "pca")
if [ "${output}" = "pca" ]; then
    printf 'ok 1 - pca init type accepted\n'
else
    printf 'not ok 1 - pca init type returned %s\n' "${output}"
    exit 1
fi

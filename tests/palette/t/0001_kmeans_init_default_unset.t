#!/bin/sh
# TAP test: default k-means init type when env is unset.

set -euxv

test_name=$(basename "$0")
test_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
category_name=$(basename "$(dirname "${test_dir}")")
artifact_root=${ARTIFACT_ROOT:-"$(pwd)/_artifacts"}
artifact_dir="${artifact_root}/${category_name}/${test_name}"

mkdir -p "${artifact_dir}"

script_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
. "${script_dir}/kmeans_init_common.sh"

echo "1..1"

output=$(run_kmeans_init "")
if [ "${output}" = "none" ]; then
    printf 'ok 1 - unset env defaults to none\n'
else
    printf 'not ok 1 - unset env produced %s\n' "${output}"
    exit 1
fi
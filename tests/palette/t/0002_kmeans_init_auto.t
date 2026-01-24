#!/bin/sh
# TAP test: auto k-means init type falls back to none.

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

output=$(run_kmeans_init "auto")
if [ "${output}" = "none" ]; then
    printf 'ok 1 - auto falls back to none\n'
else
    printf 'not ok 1 - auto produced %s\n' "${output}"
    exit 1
fi
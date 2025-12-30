#!/bin/sh
# TAP test: unknown k-means init type falls back to none.

set -euxv

test_name=$(basename "$0")
artifact_root=${ARTIFACT_ROOT:-"$(pwd)/_artifacts"}
artifact_dir="${artifact_root}/${test_name}"

mkdir -p "${artifact_dir}"

script_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
. "${script_dir}/kmeans_init_common.sh"

echo "1..1"

output=$(run_kmeans_init "unknown")
if [ "${output}" = "none" ]; then
    printf 'ok 1 - unknown value falls back to none\n'
else
    printf 'not ok 1 - unknown value produced %s\n' "${output}"
    exit 1
fi

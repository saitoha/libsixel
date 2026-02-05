#!/bin/sh
# TAP test: default k-means init type when env is unset.

set -eux



script_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
. "${TOP_SRCDIR}/tests/lib/sh/palette/kmeans_init_common.sh"

echo "1..1"
set -v

output=$(run_kmeans_init "")
if [ "${output}" = "none" ]; then
    printf 'ok 1 - unset env defaults to none\n'
else
    printf 'not ok 1 - unset env produced %s\n' "${output}"
    exit 1
fi

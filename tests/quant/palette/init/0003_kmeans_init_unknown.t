#!/bin/sh
# TAP test: unknown k-means init type falls back to none.

set -eux



script_dir=$(CDPATH=; cd "${0%[/\\]*}" && pwd)
. "${TOP_SRCDIR}/tests/lib/sh/palette/kmeans_init_common.sh"

echo "1..1"
set -v

output=$(run_kmeans_init "unknown")
if [ "${output}" = "none" ]; then
    printf 'ok 1 - unknown value falls back to none\n'
else
    printf 'not ok 1 - unknown value produced %s\n' "${output}"
    exit 1
fi

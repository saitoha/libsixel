#!/bin/sh
# Verify -h is rejected because only -H is documented and supported.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

printf '1..1\n'
set -v

run_lsqa -h >/dev/null && {
    echo "not ok" 1 "lsqa -h should fail"
    exit 0
}

echo "ok" 1 "lsqa -h is rejected"
exit 0

#!/bin/sh
# TAP test: PNGSuite basic samples converted with width 32.

set -eux

output_dir="${ARTIFACT_LOCAL_DIR}"


script_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"
. "${TOP_SRCDIR}/tests/lib/sh/loader/pngsuite_common.sh"

status=0



ensure_pngsuite_prereqs

echo "1..1"
set -v

if convert_pngsuite_group "${pngsuite_basic}" "basic samples" "-w32" "${output_dir}" ""; then
    pass 1 "basic samples with width 32"
else
    fail 1 "basic samples with width 32"
fi

exit "${status}"

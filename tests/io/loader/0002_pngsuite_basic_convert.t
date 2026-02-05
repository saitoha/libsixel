#!/bin/sh
# TAP test: PNGSuite basic samples convert with default options.

set -eux

script_dir=$(CDPATH=; cd "${0%[/\\]*}" && pwd)
. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"
. "${TOP_SRCDIR}/tests/lib/sh/loader/pngsuite_common.sh"

status=0

ensure_pngsuite_prereqs

echo "1..1"
set -v

if convert_pngsuite_group "${pngsuite_basic}" "basic samples" "" "${ARTIFACT_LOCAL_DIR}" ""; then
    pass 1 "basic samples convert"
else
    fail 1 "basic samples convert"
fi

exit "${status}"

#!/bin/sh
# TAP test: PNGSuite basic samples cropped to 16x16 at +8+8.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"
. "${TOP_SRCDIR}/tests/lib/sh/loader/pngsuite_common.sh"

status=0

ensure_pngsuite_prereqs

echo "1..1"
set -v

if convert_pngsuite_group "${pngsuite_basic}" "basic samples" "-c16x16+8+8" "${ARTIFACT_LOCAL_DIR}" ""; then
    pass 1 "basic samples cropped"
else
    fail 1 "basic samples cropped"
fi

exit "${status}"

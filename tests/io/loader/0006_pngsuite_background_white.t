#!/bin/sh
# TAP test: PNGSuite background samples rendered with white background.

set -eux

script_dir=$(CDPATH=; cd "${0%[/\\]*}" && pwd)
. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"
. "${TOP_SRCDIR}/tests/lib/sh/loader/pngsuite_common.sh"

status=0

ensure_pngsuite_prereqs

echo "1..1"
set -v

if convert_pngsuite_group "${pngsuite_background}" "background samples" "-B#fff" "${ARTIFACT_LOCAL_DIR}" ""; then
    pass 1 "background samples with white background"
else
    fail 1 "background samples with white background"
fi

exit "${status}"

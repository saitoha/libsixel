#!/bin/sh
# TAP test verifying sixel2png reports version and exits successfully.

set -eux

output_dir="${ARTIFACT_LOCAL_DIR}"


script_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

status=0

ensure_converter_available "SIXEL2PNG" "${SIXEL2PNG_PATH}" "sixel2png"



echo "1..1"
set -v

version_output="${output_dir}/version.txt"
if run_sixel2png -V >"${version_output}"; then
    if grep -Eq '^sixel2png ' "${version_output}"; then
        pass 1 "-V prints version"
    else
        fail 1 "version header missing"
    fi
else
    fail 1 "-V exited with failure"
fi

exit "${status}"

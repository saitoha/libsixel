#!/bin/sh
# TAP test: PNGSuite basic samples converted with width 32.

set -eux

test_name=$(basename "$0")
test_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
category_name=$(basename "$(dirname "${test_dir}")")
artifact_root=${ARTIFACT_ROOT:-"$(pwd)/_artifacts"}
artifact_test_dir=$(dirname "$0")
artifact_dir="${artifact_root}/${artifact_test_dir}/${test_name}"
log_file="${artifact_dir}/pngsuite.log"
output_dir="${artifact_dir}/outputs"

mkdir -p "${artifact_dir}" "${output_dir}"

script_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"
. "${TOP_SRCDIR}/tests/lib/sh/loader/pngsuite_common.sh"

status=0



ensure_pngsuite_prereqs

echo "1..1"
set -v

if convert_pngsuite_group "${pngsuite_basic}" "basic samples" "-w32" "${output_dir}" "${log_file}"; then
    pass 1 "basic samples with width 32"
else
    fail 1 "basic samples with width 32"
fi

exit "${status}"

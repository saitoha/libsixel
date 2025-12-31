#!/bin/sh
# TAP test: PNGSuite basic samples cropped to 16x16 at +8+8.

set -euxv

test_name=$(basename "$0")
test_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
category_name=$(basename "$(dirname "${test_dir}")")
artifact_root=${ARTIFACT_ROOT:-"$(pwd)/_artifacts"}
artifact_dir="${artifact_root}/${category_name}/${test_name}"
log_file="${artifact_dir}/pngsuite.log"
output_dir="${artifact_dir}/outputs"

mkdir -p "${artifact_dir}" "${output_dir}"

script_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
. "${script_dir}/../../common/t/0001_converters_common.t"
. "${script_dir}/../pngsuite_common.sh"

status=0

pass() {
    printf 'ok %s - %s\n' "$1" "$2"
}

fail() {
    printf 'not ok %s - %s\n' "$1" "$2"
    status=1
}

ensure_pngsuite_prereqs

echo "1..1"

if convert_pngsuite_group "${pngsuite_basic}" "basic samples" "-c16x16+8+8" "${output_dir}" "${log_file}"; then
    pass 1 "basic samples cropped"
else
    fail 1 "basic samples cropped"
fi

exit "${status}"
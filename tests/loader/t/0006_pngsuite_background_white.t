#!/bin/sh
# TAP test: PNGSuite background samples rendered with white background.

set -euxv

test_name=$(basename "$0")
artifact_root=${ARTIFACT_ROOT:-"$(pwd)/_artifacts"}
artifact_dir="${artifact_root}/${test_name}"
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

echo "1..1"

ensure_pngsuite_prereqs

if convert_pngsuite_group "${pngsuite_background}" "background samples" "-B#fff" "${output_dir}" "${log_file}"; then
    pass 1 "background samples with white background"
else
    fail 1 "background samples with white background"
fi

exit "${status}"

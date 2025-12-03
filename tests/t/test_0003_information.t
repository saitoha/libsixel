#!/bin/sh
# TAP test ensuring informational img2sixel commands execute successfully.

set -eu

test_name=$(basename "$0")
artifact_root=${ARTIFACT_ROOT:-"$(pwd)/_artifacts"}
artifact_dir="${artifact_root}/${test_name}"
log_file="${artifact_dir}/information.log"

mkdir -p "${artifact_dir}"

script_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
. "${script_dir}/converters-common.t"

status=0

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

pass() {
    printf 'ok %s - %s\n' "$1" "$2"
}

fail() {
    printf 'not ok %s - %s\n' "$1" "$2"
    status=1
}

echo "1..2"

if run_img2sixel -H >"${artifact_dir}/help.txt" 2>>"${log_file}"; then
    pass 1 "help output available"
else
    fail 1 "help output failed"
fi

if run_img2sixel -V >"${artifact_dir}/version.txt" 2>>"${log_file}"; then
    pass 2 "version output available"
else
    fail 2 "version output failed"
fi

exit "${status}"

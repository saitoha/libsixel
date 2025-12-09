#!/bin/sh
# TAP test verifying incompatible img2sixel option combinations are rejected
# without emitting stray output.

# Enable strict mode with verbose tracing for diagnostics.
set -euxv

test_name=$(basename "$0")
artifact_root=${ARTIFACT_ROOT:-"$(pwd)/_artifacts"}
artifact_dir="${artifact_root}/${test_name}"
log_file="${artifact_dir}/invalid-combinations.log"
tmp_dir="${artifact_dir}/tmp"

mkdir -p "${tmp_dir}"

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

echo "1..4"

require_file "${images_dir}/snake.jpg"
require_file "${images_dir}/snake.png"

expect_failure() {
    output_file=$(make_temp_file "${tmp_dir}" "capture.invalid")

    if run_img2sixel "$@" </dev/null >"${output_file}" 2>>"${log_file}"; then
        :
    fi

    if [ -s "${output_file}" ]; then
        rm -f "${output_file}"
        return 1
    fi

    rm -f "${output_file}"
    return 0
}

if expect_failure -p16 -e "${images_dir}/snake.jpg"; then
    pass 1 "palette and encode flags conflict"
else
    fail 1 "palette and encode flags allowed"
fi

if expect_failure -I -p8 "${images_dir}/snake.png"; then
    pass 2 "inspect and palette options conflict"
else
    fail 2 "inspect and palette options allowed"
fi

if expect_failure -p64 -bxterm256 "${images_dir}/snake.png"; then
    pass 3 "palette size conflicts with terminal preset"
else
    fail 3 "palette size accepted with terminal preset"
fi

if expect_failure -8 -P "${images_dir}/snake.png"; then
    pass 4 "8-bit output conflicts with palette dump"
else
    fail 4 "8-bit output allowed with palette dump"
fi

exit "${status}"

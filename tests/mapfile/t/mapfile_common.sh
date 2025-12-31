#!/bin/sh
# Shared helpers for mapfile TAP tests.

set -eu

# Initialize directories for artifacts and logs. The caller must pass the
# current test name to keep outputs isolated between TAP files.
setup_mapfile_dirs() {
    test_name=$1
    test_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
    category_name=$(basename "$(dirname "${test_dir}")")

    artifact_root=${ARTIFACT_ROOT:-"$(pwd)/_artifacts"}
    artifact_dir="${artifact_root}/${category_name}/${test_name}"
    log_file="${artifact_dir}/mapfile.log"
    output_dir="${artifact_dir}/outputs"
    tmp_dir="${artifact_dir}/tmp"

    mkdir -p "${output_dir}" "${tmp_dir}"
}

# Load shared converter helpers and ensure the img2sixel binary is available
# for palette import/export tests.
load_mapfile_prereqs() {
    script_dir=$1

    . "${script_dir}/../../common/t/0001_converters_common.t"

    status=0
    ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

    snake_png="${images_dir}/snake.png"
    require_file "${snake_png}"
}

pass() {
    printf 'ok 1 - %s\n' "$1"
}

fail() {
    printf 'not ok 1 - %s\n' "$1"
    status=1
}

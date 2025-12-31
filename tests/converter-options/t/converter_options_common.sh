#!/bin/sh
# Shared helpers for converter option TAP tests.

# Enable tracing for diagnostics while preserving caller-specified -e/-u flags.
set -xv

converter_helper_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
. "${converter_helper_dir}/../../common/t/0001_converters_common.t"

setup_converter_options_env() {
    test_name=$1

    artifact_root=${ARTIFACT_ROOT:-"$(pwd)/_artifacts"}
    artifact_dir="${artifact_root}/${test_name}"
    log_file="${artifact_dir}/conversion-options.log"
    tmp_dir="${artifact_dir}/tmp"
    output_dir="${artifact_dir}/outputs"

    mkdir -p "${artifact_dir}" "${tmp_dir}" "${output_dir}"
}

pass() {
    printf 'ok %s - %s\n' "$1" "$2"
}

fail() {
    printf 'not ok %s - %s\n' "$1" "$2"
    status=1
}

ensure_img2sixel_available() {
    ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"
}

converter_expected_dcs_crc="302131327e2d2131327e1b5c"

converter_dcs_checksum() {
    scale_args=$1

    checksum=$(printf '\033Pq"1;1;1;1!6~\033\\' \
        | run_img2sixel -rne ${scale_args} 2>>"${log_file}" \
        | tr '#' '\n' | tail -n +3 \
        | od -An -tx1 | tr -d ' \n') || checksum=""

    printf '%s' "${checksum}"
}

converter_check_dcs_crc() {
    case_no=$1
    scale_args=$2
    description=$3

    digest=$(converter_dcs_checksum "${scale_args}")

    if [ -z "${digest}" ]; then
        fail "${case_no}" "${description} (no checksum produced)"
        return
    fi

    if [ "x${digest}" = "x${converter_expected_dcs_crc}" ]; then
        pass "${case_no}" "${description}"
    else
        fail "${case_no}" "${description}"
    fi
}

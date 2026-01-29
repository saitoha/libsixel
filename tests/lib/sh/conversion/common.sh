#!/bin/sh
# Shared helpers for converter TAP tests grouped by functionality.

# Enable tracing for diagnostics while preserving caller-specified -e/-u flags.
initial_shell_flags=$-
set -xv
case ${initial_shell_flags} in
*e*) set -e ;;
esac
case ${initial_shell_flags} in
*u*) set -u ;;
esac

conversion_common_path=${conversion_common_path:-"$0"}
conversion_helper_dir=${CONVERSION_HELPER_DIR-}
if [ -z "${conversion_helper_dir}" ]; then
    conversion_helper_dir=$(CDPATH=; cd "$(dirname "${conversion_common_path}")" && pwd)
fi
. "${conversion_helper_dir}/../common/tap.sh"
. "${conversion_helper_dir}/../../../_lib/sh/common.sh"

setup_conversion_env() {
    test_name=$1
    test_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
    category_name=$(basename "$(dirname "${test_dir}")")

    artifact_root=${ARTIFACT_ROOT:-"$(pwd)/_artifacts"}
    artifact_dir="${artifact_root}/${category_name}/${test_name}"
    log_file="${artifact_dir}/conversion.log"
    tmp_dir="${artifact_dir}/tmp"
    output_dir="${artifact_dir}/outputs"

    mkdir -p "${artifact_dir}" "${tmp_dir}" "${output_dir}"
}

ensure_img2sixel_available() {
    ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"
}

expected_dcs_crc="302131327e2d2131327e1b5c"

conversion_dcs_checksum() {
    scale_args=$1

    checksum=$(printf '\033Pq"1;1;1;1!6~\033\\' \
        | run_img2sixel -rne ${scale_args} 2>>"${log_file}" \
        | tr '#' '\n' | tail -n +3 \
        | od -An -tx1 | tr -d ' \n') || checksum=""

    printf '%s' "${checksum}"
}

check_dcs_crc() {
    case_no=$1
    scale_args=$2
    description=$3

    digest=$(conversion_dcs_checksum "${scale_args}")

    if [ -z "${digest}" ]; then
        fail "${case_no}" "${description} (no checksum produced)"
        return
    fi

    if [ "x${digest}" = "x${expected_dcs_crc}" ]; then
        pass "${case_no}" "${description}"
    else
        fail "${case_no}" "${description}"
    fi
}

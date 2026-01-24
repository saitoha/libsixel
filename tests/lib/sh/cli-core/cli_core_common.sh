#!/bin/sh
# Shared helpers for CLI core TAP tests. This sets up artifact directories,
# loads common converter helpers, and provides pass/fail utilities.

cli_core_setup() {
    log_basename=$1

    test_name=$(basename "$0")
    test_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
    category_name=$(basename "$(dirname "${test_dir}")")
    artifact_root=${ARTIFACT_ROOT:-"$(pwd)/_artifacts"}
    artifact_dir="${artifact_root}/${category_name}/${test_name}"
    log_file="${artifact_dir}/${log_basename}.log"
    output_dir="${artifact_dir}/outputs"
    tmp_dir="${artifact_dir}/tmp"

    mkdir -p "${output_dir}" "${tmp_dir}"

    cli_core_common_path=${cli_core_common_path:-"$0"}
    helper_dir=${CLI_CORE_HELPER_DIR-}
    if [ -z "${helper_dir}" ]; then
        helper_dir=$(CDPATH=; cd "$(dirname "${cli_core_common_path}")" && pwd)
    fi

    . "${helper_dir}/../../../common/t/0001_converters_common.t"

    status=0
}

cli_core_pass() {
    printf 'ok %s - %s\n' "$1" "$2"
}

cli_core_fail() {
    printf 'not ok %s - %s\n' "$1" "$2"
    status=1
}

cli_core_skip() {
    reason="skipped"
    if [ "$#" -ge 3 ]; then
        reason=$3
    fi

    printf 'ok %s - %s # SKIP %s\n' "$1" "$2" "${reason}"
}

cli_core_expect_img2sixel_rejection() {
    index=$1
    description=$2
    shift 2

    output_file=$(make_temp_file "${tmp_dir}" "capture.invalid")
    if run_img2sixel "$@" </dev/null >"${output_file}" 2>>"${log_file}"; then
        cmd_status=0
    else
        cmd_status=$?
    fi

    if [ "${cmd_status}" -eq 0 ]; then
        cli_core_fail "${index}" "unexpected success: ${description}"
    elif [ -s "${output_file}" ]; then
        cli_core_fail "${index}" "unexpected output: ${description}"
    else
        cli_core_pass "${index}" "${description}"
    fi

    rm -f "${output_file}"
}

cli_core_pick_comparator() {
    comparator_cmd=""
    if command -v cmp >/dev/null 2>&1; then
        comparator_cmd="cmp -s"
    elif command -v diff >/dev/null 2>&1; then
        comparator_cmd="diff -q"
    fi
}

cli_core_files_identical() {
    if [ -z "${comparator_cmd:-}" ]; then
        return 1
    fi

    if ${comparator_cmd} "$1" "$2"; then
        return 0
    fi

    return 1
}

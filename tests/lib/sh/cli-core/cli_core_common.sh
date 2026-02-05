#!/bin/sh
# Shared helpers for CLI core TAP tests. This sets up artifact directories,
# loads common converter helpers, and provides pass/fail utilities.

cli_core_setup() {
    log_basename=$1

    cli_core_common_path=${cli_core_common_path:-"$0"}
    helper_dir=${CLI_CORE_HELPER_DIR-}
    if [ -z "${helper_dir}" ]; then
        helper_dir=$(CDPATH=; cd "$(dirname "${cli_core_common_path}")" && pwd)
    fi

    . "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

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

    output_file=$(make_temp_file "${ARTIFACT_LOCAL_DIR}" "capture.invalid")
    if run_img2sixel "$@" </dev/null >"${output_file}"; then
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

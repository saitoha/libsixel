#!/bin/sh
# SPDX-License-Identifier: MIT
# Shared staticcheck dispatch; the suite and its regression control call this
# exact function. The caller owns src_root, tmpdir and the summary counters.

run_case_tap() {
    : "${src_root:?}" "${tmpdir:?}" "${total:?}"
    case_name=$1
    shift
    index=$((index + 1))
    log_path="$tmpdir/${index}.tap"
    error_log="$tmpdir/${index}.stderr"
    verdict_log="$tmpdir/${index}.verdict"
    printf '==> [%02d/%02d] %s\n' "$index" "$total" "$case_name"

    # TAP is stdout. In particular, shell -v/-x output on stderr may contain
    # literal not-ok, plan or bailout text without representing a test result.
    if "$@" >"$log_path" 2>"$error_log"; then
        case_rc=0
    else
        case_rc=$?
    fi
    if LC_ALL=C awk -f "$src_root/tests/_static/awk/tap-status.awk" \
            "$log_path" >"$verdict_log"; then
        tap_rc=0
    else
        tap_rc=$?
    fi
    if test "$case_rc" -eq 0 && test "$tap_rc" -eq 0; then
        pass_count=$((pass_count + 1))
        printf 'PASS %s\n' "$case_name"
        return 0
    fi
    if test "$case_rc" -eq 0 && test "$tap_rc" -eq 77; then
        skip_count=$((skip_count + 1))
        printf 'SKIP %s\n' "$case_name"
        cat "$log_path"
        return 0
    fi

    fail_count=$((fail_count + 1))
    printf 'FAIL %s (child status %s, TAP status %s)\n' \
        "$case_name" "$case_rc" "$tap_rc"
    # Preserve actual TAP, diagnostics and parser reasons, including the
    # failing result that the previous display filter discarded.
    cat "$log_path" "$error_log" "$verdict_log"
    return 1
}

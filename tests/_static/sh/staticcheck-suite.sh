#!/bin/sh
# Run static checks in deterministic order and present concise plain logs.

set -eu

src_root=${1:-}
build_root=${2:-}
actionlint_bin=${3:-}
shellcheck_driver=${4:-}
shellcheck_bin=${5:-}
codespell_bin=${6:-}
python_bin=${7:-}

test -n "$src_root" || {
    echo "Usage: $0 <src_root> <build_root> <actionlint> <shellcheck_driver> <shellcheck> <codespell> <python>" >&2
    exit 2
}

test -n "$build_root" || build_root="$src_root"

tmpdir=$(mktemp -d "${TMPDIR:-/tmp}/libsixel-staticcheck-suite-XXXXXX")
trap 'rm -rf "$tmpdir"' EXIT HUP INT TERM

TOP_SRCDIR=${TOP_SRCDIR:-$src_root}
TOP_BUILDDIR=${TOP_BUILDDIR:-$build_root}
ARTIFACT_ROOT=${ARTIFACT_ROOT:-$build_root/tests/_artifacts}
export TOP_SRCDIR TOP_BUILDDIR ARTIFACT_ROOT

total=20
index=0
pass_count=0
skip_count=0
fail_count=0

strip_tap_log() {
    log_path=$1
    awk '
    /^1\.\.0[[:space:]]*#[[:space:]]*[Ss][Kk][Ii][Pp][[:space:]]*/ {
        line=$0
        sub(/^1\.\.0[[:space:]]*#[[:space:]]*[Ss][Kk][Ii][Pp][[:space:]]*/, "SKIP: ", line)
        print line
        next
    }
    /^ok[[:space:]]+[0-9]+[[:space:]]*#[[:space:]]*[Ss][Kk][Ii][Pp][[:space:]]*/ {
        line=$0
        sub(/^ok[[:space:]]+[0-9]+[[:space:]]*#[[:space:]]*[Ss][Kk][Ii][Pp][[:space:]]*/, "SKIP: ", line)
        print line
        next
    }
    /^1\.\.[0-9]+([[:space:]]|$)/ { next }
    /^(not[[:space:]]+)?ok([[:space:]]|$)/ { next }
    {
        line=$0
        sub(/^#[[:space:]]?/, "", line)
        if (line != "") {
            print line
        }
    }
    ' "$log_path"
}

is_tap_skip() {
    log_path=$1
    awk '
    /^1\.\.0[[:space:]]*#[[:space:]]*[Ss][Kk][Ii][Pp]([[:space:]]|$)/ { found=1 }
    /^ok[[:space:]]+[0-9]+[[:space:]]*#[[:space:]]*[Ss][Kk][Ii][Pp]([[:space:]]|$)/ { found=1 }
    END { exit found ? 0 : 1 }
    ' "$log_path"
}

tool_is_available() {
    tool_path=$1
    test -n "$tool_path" || return 1
    test -x "$tool_path" && return 0
    command -v "$tool_path" >/dev/null 2>&1
}

run_case_tap() {
    case_name=$1
    shift

    index=$((index + 1))
    log_path="$tmpdir/${index}.log"
    filtered_log="$tmpdir/${index}.filtered"

    printf '==> [%02d/%02d] %s\n' "$index" "$total" "$case_name"

    if "$@" >"$log_path" 2>&1; then
        case_rc=0
    else
        case_rc=$?
    fi

    strip_tap_log "$log_path" > "$filtered_log"

    if test "$case_rc" -eq 0; then
        if is_tap_skip "$log_path"; then
            skip_count=$((skip_count + 1))
            printf 'SKIP %s\n' "$case_name"
            if test -s "$filtered_log"; then
                cat "$filtered_log"
            fi
        else
            pass_count=$((pass_count + 1))
            printf 'PASS %s\n' "$case_name"
        fi
        return 0
    fi

    fail_count=$((fail_count + 1))
    printf 'FAIL %s\n' "$case_name"
    if test -s "$filtered_log"; then
        cat "$filtered_log"
    else
        sed -n '1,120p' "$log_path"
    fi
    return "$case_rc"
}

run_case_plain() {
    case_name=$1
    shift

    index=$((index + 1))
    log_path="$tmpdir/${index}.plain.log"
    printf '==> [%02d/%02d] %s\n' "$index" "$total" "$case_name"

    if "$@" >"$log_path" 2>&1; then
        if test -s "$log_path"; then
            cat "$log_path"
        fi
        pass_count=$((pass_count + 1))
        printf 'PASS %s\n' "$case_name"
        return 0
    fi

    if test -s "$log_path"; then
        cat "$log_path"
    fi
    fail_count=$((fail_count + 1))
    printf 'FAIL %s\n' "$case_name"
    return 1
}

run_case_skip() {
    case_name=$1
    reason=$2

    index=$((index + 1))
    skip_count=$((skip_count + 1))
    printf '==> [%02d/%02d] %s\n' "$index" "$total" "$case_name"
    printf 'SKIP %s\n' "$case_name"
    printf 'SKIP: %s\n' "$reason"
}

fail_and_exit() {
    rc=$1
    printf 'staticcheck summary: total=%d pass=%d skip=%d fail=%d\n' \
        "$index" "$pass_count" "$skip_count" "$fail_count"
    exit "$rc"
}

run_case_tap "staticcheck-private-includes" \
    "$src_root/tests/_static/sh/staticcheck-private-includes.sh" \
    "$src_root" "$python_bin" || fail_and_exit $?

run_case_tap "staticcheck-makefile-recipes" \
    "$src_root/tests/_static/sh/staticcheck-makefile-recipes.sh" \
    "$src_root" || fail_and_exit $?

run_case_tap "staticcheck-meson-palette-sources" \
    "$src_root/tests/_static/sh/staticcheck-meson-palette-sources.sh" \
    "$src_root" || fail_and_exit $?

run_case_tap "staticcheck-amalgamation-static-symbols" \
    "$src_root/tests/_static/sh/staticcheck-amalgamation-static-symbols.sh" \
    "$src_root" || fail_and_exit $?

run_case_tap "staticcheck-kmedoids-suboption-env-sync" \
    "$src_root/tests/_static/sh/staticcheck-kmedoids-suboption-env-sync.sh" \
    "$src_root" || fail_and_exit $?

run_case_tap "staticcheck-test-runner-dllexport" \
    "$src_root/tests/_static/sh/staticcheck-test-runner-dllexport.sh" \
    "$src_root" || fail_and_exit $?

run_case_tap "staticcheck-binding-loader-setopt-sync" \
    "$src_root/tests/_static/sh/staticcheck-binding-loader-setopt-sync.sh" \
    "$src_root" || fail_and_exit $?

run_case_tap "staticcheck-python-compileall" \
    "$src_root/tests/_static/sh/staticcheck-python-compileall.sh" \
    "$src_root" "$python_bin" || fail_and_exit $?

run_case_tap "staticcheck-actionlint" \
    "$src_root/tests/_static/sh/staticcheck-actionlint.sh" \
    "$src_root" "$actionlint_bin" || fail_and_exit $?

if tool_is_available "$shellcheck_bin"; then
    run_case_plain "staticcheck-shellcheck" \
        env SIXEL_STATICCHECK_MODE=plain \
        "$src_root/tests/_static/sh/staticcheck-shellcheck.sh" \
        "$shellcheck_driver" "$src_root" "$shellcheck_bin" || fail_and_exit $?
else
    run_case_skip "staticcheck-shellcheck" "shellcheck not found"
fi

if tool_is_available "$codespell_bin"; then
    run_case_plain "staticcheck-codespell" \
        env SIXEL_STATICCHECK_MODE=plain \
        "$src_root/tests/_static/sh/staticcheck-codespell.sh" \
        "$src_root" "$codespell_bin" || fail_and_exit $?
else
    run_case_skip "staticcheck-codespell" "codespell not found"
fi

run_case_tap "staticcheck-test-plan-single" \
    "$src_root/tests/_static/sh/staticcheck-test-plan-single.sh" \
    "$src_root" || fail_and_exit $?

run_case_tap "staticcheck-artifact-local-dir-mkdir" \
    "$src_root/tests/_static/sh/staticcheck-artifact-local-dir-mkdir.sh" \
    "$src_root" || fail_and_exit $?

run_case_tap "staticcheck-test-no-grep-awk" \
    "$src_root/tests/_static/sh/staticcheck-test-no-grep-awk.sh" \
    "$src_root" || fail_and_exit $?

run_case_tap "staticcheck-c-header-modeline" \
    "$src_root/tests/_static/sh/staticcheck-c-header-modeline.sh" \
    "$src_root" || fail_and_exit $?

run_case_tap "staticcheck-build-doc-configure-help" \
    "$src_root/tests/_static/sh/staticcheck-build-doc-configure-help.sh" \
    "$src_root" || fail_and_exit $?

run_case_tap "staticcheck-build-doc-meson-options" \
    "$src_root/tests/_static/sh/staticcheck-build-doc-meson-options.sh" \
    "$src_root" || fail_and_exit $?

run_case_tap "staticcheck-docs-help-vs-man" \
    "$src_root/tests/_static/sh/staticcheck-docs-help-vs-man.sh" \
    "$src_root" || fail_and_exit $?

run_case_tap "staticcheck-docs-man-vs-bash-completion" \
    "$src_root/tests/_static/sh/staticcheck-docs-man-vs-bash-completion.sh" \
    "$src_root" || fail_and_exit $?

run_case_tap "staticcheck-docs-envvars-help-table" \
    "$src_root/tests/_static/sh/staticcheck-docs-envvars-help-table.sh" \
    "$src_root" || fail_and_exit $?

printf 'staticcheck summary: total=%d pass=%d skip=%d fail=%d\n' \
    "$index" "$pass_count" "$skip_count" "$fail_count"

exit 0

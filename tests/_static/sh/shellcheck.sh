#!/bin/sh
# Shared ShellCheck and style checker for TAP shell tests.
#
# Usage:
#   tests/_static/sh/shellcheck.sh <repo_root>
#
# Environment:
#   TEST_FILES: optional whitespace-separated list of test paths to run
#               project-specific style checks on.
#               ShellCheck itself always runs against all tests/*.t files.

set -eu

repo_root=${1:-}
test -n "${repo_root}" || {
    echo "Usage: $0 <repo_root>" >&2
    exit 2
}

script_dir=$(CDPATH="" cd -- "$(dirname -- "$0")" && pwd)
cd "${script_dir}/../../.."

test -d "${repo_root}/tests" || {
    echo "ERROR: invalid repository root: ${repo_root}" >&2
    exit 2
}

shellcheck_cmd=${SHELLCHECK_CMD:-shellcheck}
if ! command -v "${shellcheck_cmd}" >/dev/null 2>&1; then
    echo "ERROR: shellcheck not found: ${shellcheck_cmd}" >&2
    exit 2
fi

tmpfile=$(mktemp "${TMPDIR:-/tmp}/libsixel-shellcheck-files-XXXXXX")
tmpdir=$(mktemp -d "${TMPDIR:-/tmp}/libsixel-shellcheck-run-XXXXXX")
cleanup() {
    rm -f "$tmpfile"
    rm -rf "$tmpdir"
}
trap cleanup EXIT HUP INT TERM

find "${repo_root}/tests" -type f -name '*.t' | LC_ALL=C sort > "$tmpfile"
total=$(wc -l < "$tmpfile" | awk '{print $1}')
jobs=${STATICCHECK_JOBS:-}
if test -z "$jobs"; then
    jobs=$(getconf _NPROCESSORS_ONLN 2>/dev/null || printf '4')
fi
case "$jobs" in
    ''|*[!0-9]*|0)
        jobs=1
        ;;
esac

echo "Running ShellCheck for tests/*.t"
ARTIFACT_LOCAL_DIR=${ARTIFACT_LOCAL_DIR:-"${repo_root}/tests/_artifacts"}
TOP_SRCDIR=${TOP_SRCDIR:-"${repo_root}"}
export ARTIFACT_LOCAL_DIR TOP_SRCDIR
progress_is_tty=0
test -t 1 && progress_is_tty=1

failed=0
if test "$total" -eq 0; then
    echo "[shellcheck] no test files found"
else
    queue_file="$tmpdir/queue.bin"
    result_fifo="$tmpdir/result.fifo"
    failed_list="$tmpdir/failed.list"
    tab_char=$(printf '\t')
    index=0
    while IFS= read -r test_file; do
        test -n "${test_file}" || continue
        index=$((index + 1))
        printf '%s\0%s\0' "$index" "$test_file" >> "$queue_file"
    done < "$tmpfile"

    mkfifo "$result_fifo"
    : > "$failed_list"

    done_count=0
    ok_count=0
    failed_count=0
    xargs_status=0

    export SHELLCHECK_CMD_INNER="$shellcheck_cmd"
    export REPO_ROOT_INNER="$repo_root"
    export SHELLCHECK_LOG_DIR="$tmpdir"
    # shellcheck disable=SC2016
    xargs -0 -n 2 -P "$jobs" sh -c '
        item_id=$1
        test_file=$2
        rel_file=${test_file#${REPO_ROOT_INNER}/}
        log_path="${SHELLCHECK_LOG_DIR}/${item_id}.log"
        if "$SHELLCHECK_CMD_INNER" -x -P "${REPO_ROOT_INNER}" "$test_file" >"$log_path" 2>&1; then
            printf "OK\t%s\t%s\n" "$item_id" "$rel_file"
        else
            printf "NG\t%s\t%s\n" "$item_id" "$rel_file"
        fi
    ' sh < "$queue_file" > "$result_fifo" &
    worker_pid=$!
    unset SHELLCHECK_CMD_INNER REPO_ROOT_INNER SHELLCHECK_LOG_DIR

    while IFS="$tab_char" read -r result item_id rel_file; do
        test -n "$result" || continue
        done_count=$((done_count + 1))
        case "$result" in
            OK)
                ok_count=$((ok_count + 1))
                ;;
            NG)
                failed_count=$((failed_count + 1))
                printf '%s\t%s\n' "$item_id" "$rel_file" >> "$failed_list"
                ;;
        esac
        percent=$((done_count * 100 / total))
        if test "$progress_is_tty" -eq 1; then
            printf '\r[shellcheck] %3d%% (%d/%d) success=%d failed=%d' \
                "$percent" "$done_count" "$total" "$ok_count" "$failed_count"
        fi
    done < "$result_fifo"

    wait "$worker_pid" || xargs_status=$?
    if test "$progress_is_tty" -eq 1; then
        printf '\n'
    fi

    case "$xargs_status" in
        0|123)
            ;;
        *)
            echo "ERROR: xargs failed while running shellcheck (status=$xargs_status)"
            failed=1
            ;;
    esac

    printf '[shellcheck] summary: success=%d failed=%d total=%d\n' \
        "$ok_count" "$failed_count" "$total"

    if test -s "$failed_list"; then
        while IFS="$tab_char" read -r item_id rel_file; do
            test -n "$item_id" || continue
            echo "shellcheck failure: $rel_file"
            sed -n '1,120p' "$tmpdir/$item_id.log"
        done < "$failed_list"
        failed=1
    fi
fi

echo "Checking shell test policy rules"
if_hits=$(
    find "${repo_root}/tests" -name '*.t' -type f \
        -exec grep -l '^if ' {} + || true
)
test -z "${if_hits}" || {
    printf '%s\n' "${if_hits}"
    echo "ERROR: if is forbidden in tests/*.t"
    failed=1
}

func_hits=$(
    find "${repo_root}/tests" -name '*.t' -type f \
        -exec grep '^[a-z_]\+()' {} + || true
)
test -z "${func_hits}" || {
    printf '%s\n' "${func_hits}"
    echo "ERROR: shell functions are forbidden in tests/*.t"
    failed=1
}

target_files=${TEST_FILES:-}
test -n "${target_files}" || {
    echo "Skipping custom checks: set TEST_FILES='tests/.../*.t'"
    test "${failed}" -eq 0
    exit $?
}

resolved_files=''
for target_file in ${target_files}; do
    test -f "${target_file}" &&
        resolved_files="${resolved_files} ${target_file}"
    test -f "${repo_root}/${target_file}" &&
        resolved_files="${resolved_files} ${repo_root}/${target_file}"
    test -f "${repo_root}/tests/${target_file}" &&
        resolved_files="${resolved_files} ${repo_root}/tests/${target_file}"
done

test -n "${resolved_files}" || {
    echo "ERROR: TEST_FILES did not match any test file"
    failed=1
    test "${failed}" -eq 0
    exit $?
}

echo "Checking custom test style rules"

check_pattern() {
    pattern=$1
    message=$2
    # shellcheck disable=SC2086
    if rg -n --pcre2 "${pattern}" ${resolved_files}; then
        echo "ERROR: ${message}"
        failed=1
    fi
}

check_pattern '^[[:space:]]*if[[:space:]]' \
    'if is forbidden in tests/*.t'
check_pattern '^[[:space:]]*[a-z_]+\(\)' \
    'shell functions are forbidden in tests/*.t'
check_pattern_multiline() {
    pattern=$1
    message=$2
    # shellcheck disable=SC2086
    if rg -n -U --pcre2 "${pattern}" ${resolved_files}; then
        echo "ERROR: ${message}"
        failed=1
    fi
}

check_pattern_multiline '(?ms)\|\|[[:space:]]*\{(?:(?!^[[:space:]]*}[[:space:]]*$).)*\|\|[[:space:]]*\{' \
    'nested || { ... } blocks are forbidden in tests/*.t'
check_pattern '^[[:space:]]*\[[^]]*\]' \
    "use test syntax instead of bracket syntax in tests/*.t"
check_pattern '^[^#]*[^0-9]2>[[:space:]]*""' \
    'redirecting stderr to "" is forbidden in tests/*.t'
check_pattern '^[^#]*(^|[^0-9])>[[:space:]]*""' \
    'redirecting stdout to "" is forbidden in tests/*.t'

test "${failed}" -eq 0

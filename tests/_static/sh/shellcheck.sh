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
cleanup() {
    rm -f "$tmpfile"
}
trap cleanup EXIT HUP INT TERM

find "${repo_root}/tests" -type f -name '*.t' | LC_ALL=C sort > "$tmpfile"
total=$(wc -l < "$tmpfile" | awk '{print $1}')

echo "Running ShellCheck for tests/*.t"
ARTIFACT_LOCAL_DIR=${ARTIFACT_LOCAL_DIR:-"${repo_root}/tests/_artifacts"}
TOP_SRCDIR=${TOP_SRCDIR:-"${repo_root}"}
export ARTIFACT_LOCAL_DIR TOP_SRCDIR

failed=0
index=0
while IFS= read -r test_file; do
    test -n "${test_file}" || continue
    index=$((index + 1))
    rel_file=${test_file#${repo_root}/}
    echo "[shellcheck ${index}/${total}] ${rel_file}"
    if ! "${shellcheck_cmd}" -x -P "${repo_root}" "${test_file}"; then
        failed=1
    fi
done < "$tmpfile"

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
    exit 1
}

echo "Checking custom test style rules"
failed=0

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

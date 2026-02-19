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
    echo "skipped: shellcheck not found"
    shellcheck_cmd=''
fi

test -n "${shellcheck_cmd}" && echo "Running ShellCheck for tests/*.t"
ARTIFACT_LOCAL_DIR=${ARTIFACT_LOCAL_DIR:-"${repo_root}/tests/_artifacts"}
TOP_SRCDIR=${TOP_SRCDIR:-"${repo_root}"}
export ARTIFACT_LOCAL_DIR TOP_SRCDIR
test -n "${shellcheck_cmd}" &&
find "${repo_root}/tests" -type f -name '*.t' -exec \
    "${shellcheck_cmd}" -x -P "${repo_root}" {} +

target_files=${TEST_FILES:-}
test -n "${target_files}" || {
    echo "Skipping custom checks: set TEST_FILES='tests/.../*.t'"
    exit 0
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

check_fixed() {
    pattern=$1
    message=$2
    # shellcheck disable=SC2086
    if rg -n -F "${pattern}" ${resolved_files}; then
        echo "ERROR: ${message}"
        failed=1
    fi
}

check_pattern '^[[:space:]]*(if|elif|else|case)\b' \
    'if/elif/else/case are forbidden in tests/*.t'
check_fixed '|| {' \
    'nested || { ... } blocks are forbidden in tests/*.t'
check_pattern '^[[:space:]]*\[[^]]*\]' \
    "use test syntax instead of bracket syntax in tests/*.t"
check_pattern '^[^#]*[^0-9]2>[[:space:]]*""' \
    'redirecting stderr to "" is forbidden in tests/*.t'
check_pattern '^[^#]*(^|[^0-9])>[[:space:]]*""' \
    'redirecting stdout to "" is forbidden in tests/*.t'

test "${failed}" -eq 0

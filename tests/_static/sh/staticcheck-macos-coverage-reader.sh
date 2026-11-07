#!/bin/sh
# Execute the macOS coverage recipe with tools replaced by shell functions.
# Policy: docs/ci/design.md
# Policy: docs/testing/staticcheck.md
set -eux
command -v bash >/dev/null 2>&1 || {
    echo '1..0 # SKIP Bash is unavailable for the CI recipe'
    exit 0
}
echo '1..1'
set -v
src_root=$1
recipe=$(awk '
/^    - name: Capture coverage$/ { selected = 1; next }
selected && /^    - name:/ { exit }
selected && /^      run: \|/ { body = 1; next }
selected && body {
    sub(/^        /, "")
    gsub(/\$\{\{[^}]*\}\}/, "macos-coverage")
    print
}
' "$src_root/.github/actions/ci-steps/action.yml")
test -n "$recipe" || {
    echo 'not ok 1 - macOS coverage recipe is present'
    exit 0
}
# No compiler, Python runtime, network or coverage files are used here.
# Absolute function names intercept the tools in the isolated environment.
# Expand the stub variables only inside the child bash process.
# shellcheck disable=SC2016
script='export RUNNER_OS=macOS RUNNER_TEMP=/sixel-staticcheck
created=0
installed=0
captured=0
xcrun() {
    builtin test "$*" = "--find llvm-cov"
    printf "%s\n" /sixel-tools/llvm-cov
}
python3() {
    builtin test "$*" = "-m venv /sixel-staticcheck/libsixel-gcovr"
    created=1
}
function /sixel-staticcheck/libsixel-gcovr/bin/python {
    builtin test "$created" -eq 1
    builtin test "$*" = "-m pip install --disable-pip-version-check gcovr==8.8"
    installed=1
}
function /sixel-staticcheck/libsixel-gcovr/bin/gcovr {
    builtin test "$installed" -eq 1
    builtin test "$#" -eq 11
    builtin test "$1 $2 $3 $4 $5" = "-r . --lcov -o coverage.info"
    builtin test "$6 $7" = "--exclude .*\.gperf$"
    builtin test "$8" = "--gcov-executable"
    builtin test "$9" = "/sixel-tools/llvm-cov gcov"
    builtin test "${10}" = "--gcov-suspicious-hits-threshold=0"
    builtin test "${11}" = "--gcov-ignore-errors=no_working_dir_found"
    captured=1
    return "$READER_STATUS"
}
test() {
    case "$*" in
        "-f coverage.info") return 1 ;;
        "-s coverage.info") builtin test "$captured" -eq 1 ;;
        *) builtin test "$@" ;;
    esac
}
grep() {
    builtin test "$*" = "-q ^SF: coverage.info"
    builtin test "$captured" -eq 1
}'
script="$script
$recipe
builtin test \"\$captured\" -eq 1"
printf '%s\n' "$script" | READER_STATUS=0 bash -eu || {
    echo 'not ok 1 - macOS coverage uses the pinned LLVM reader'
    exit 0
}
result=0
printf '%s\n' "$script" | READER_STATUS=17 bash -eu || result=$?
test "$result" -eq 17 || {
    echo 'not ok 1 - macOS coverage propagates reader failure'
    exit 0
}
echo 'ok 1 - macOS coverage selects its LLVM reader and propagates failure'
exit 0

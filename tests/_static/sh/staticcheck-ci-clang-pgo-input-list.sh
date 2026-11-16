#!/bin/sh
# Exercise the merge step with command stubs and two synthetic profile paths.
set -eux
echo '1..1'
set -v

action=$1/.github/actions/ci-steps/action.yml
command -v bash >/dev/null 2>&1 || {
    echo 'ok 1 # SKIP Bash unavailable'
    exit 0
}
work_dir=$(mktemp -d "${TMPDIR:-/tmp}/libsixel-profdata.XXXXXX") || {
    echo 'not ok 1 - cannot prepare the profile list fixture'
    exit 0
}
trap 'rm -f "$work_dir/pgo/profraw.list" "$work_dir/pgo/default.profdata";
    rmdir "$work_dir/pgo" "$work_dir" 2>/dev/null || :' EXIT
{ mkdir "$work_dir/pgo" && cd "$work_dir"; } || {
    echo 'not ok 1 - cannot prepare the profile list fixture'
    exit 0
}
awk '
/^    - name:/ {
    merge = $0 == "    - name: Merge Clang PGO raw profiles"
    body = 0
}
merge && /^      run: \|$/ { body = 1; next }
body && /^        / { print substr($0, 9) }
' "$action" | bash -c '
profiles="$PWD/pgo/first.profraw
$PWD/pgo/second profile.profraw"
export profiles
find() { printf "%s\n" "$profiles"; }
llvm-profdata() {
    test "$#" -eq 3 &&
        test "$1" = merge &&
        test "$2" = "--input-files=$PWD/pgo/profraw.list" &&
        test "$3" = "-output=$PWD/pgo/default.profdata" &&
        test "$(< "$PWD/pgo/profraw.list")" = "$profiles" &&
        : > "$PWD/pgo/default.profdata"
}
export -f find llvm-profdata
bash -s && test -f "$PWD/pgo/default.profdata"
' >&2 || {
    echo 'not ok 1 - Clang PGO merges profiles through an input list'
    exit 0
}
echo 'ok 1 - Clang PGO merges profiles through an input list'
exit 0

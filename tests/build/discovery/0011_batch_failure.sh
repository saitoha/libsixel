#!/bin/sh
# Policy: docs/build/test-harness.md
# Policy: docs/testing/staticcheck.md
set -eux
src_root=$1
work=$(mktemp -d "${TMPDIR:-/tmp}/libsixel-discovery-XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
mkdir -p "$work/sample/bounded"
index=1000
while test "$index" -le 1064; do
    : > "$work/sample/bounded/${index}_case.t"
    index=$((index + 1))
done
# Fail after one successful batch while leaving a later batch available.
printf_calls=0
printf() {
    printf_calls=$((printf_calls + 1))
    test "$printf_calls" -ne 2 || return 1
    command printf "$@"
}
result=0
(
    set -- tests "$work" include newline
    # shellcheck source=build-aux/read-check-test-list.sh
    . "$src_root/build-aux/read-check-test-list.sh"
) >/dev/null || result=$?
test "$result" -ne 0

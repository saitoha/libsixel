#!/bin/sh
# Policy: docs/build/test-harness.md
# Policy: docs/testing/staticcheck.md
set -eux
src_root=$1
work=$(mktemp -d "${TMPDIR:-/tmp}/libsixel-discovery-XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
mkdir -p "$work/sample/bounded"
expected=
index=1000
while test "$index" -le 1064; do
    : > "$work/sample/bounded/${index}_case.t"
    expected="${expected}sample/bounded/${index}_case.t
"
    index=$((index + 1))
done
# Model an external printf with an argv limit, even on builtin-only hosts.
printf() {
    test "$#" -le 33 || return 1
    command printf "$@"
}
actual=$(
    set -- tests "$work" include newline
    # shellcheck source=build-aux/read-check-test-list.sh
    . "$src_root/build-aux/read-check-test-list.sh"
)
test "$actual
" = "$expected"

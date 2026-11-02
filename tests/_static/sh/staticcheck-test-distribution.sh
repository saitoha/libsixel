#!/bin/sh
# Policy: docs/testing/staticcheck.md
# Policy: docs/build/test-harness.md
set -eux
echo '1..1'
set -v
src_root=$1
work=$(mktemp -d "${TMPDIR:-/tmp}/libsixel-dist-XXXXXX")
trap 'chmod -R u+w "$work"; rm -rf "$work"' EXIT HUP INT TERM
# The observation is file distribution, so these small files are the evidence.
mkdir -p "$work/source/sub" "$work/source/data/nested" "$work/data"
printf '%s\n' source > "$work/source/sub/file.c"
printf '%s\n' generated > "$work/generated.h"
printf '%s\n' fixture > "$work/source/data/nested/fixture"
printf '%s\n' built > "$work/data/generated"
chmod -R a-w "$work/source/data"
cat > "$work/Makefile" <<MAKE
DISTFILES = source/sub/file.c generated.h data data/nested
VPATH = source
srcdir = source
top_srcdir = $src_root
top_builddir = .
distdir = package
include $src_root/build-aux/test-dist.am
dist-hook:
	@test -f package/sub/file.c
MAKE
make_bin=${MAKE:-make}
command -v gmake >/dev/null 2>&1 && make_bin=gmake
"$make_bin" -s -C "$work" distdir-am || {
    echo 'not ok 1 - test distribution failed'
    exit 0
}
# Every comparison must pass; the failure branch covers any mismatch.
# shellcheck disable=SC2015
cmp "$work/source/sub/file.c" "$work/package/sub/file.c" &&
cmp "$work/generated.h" "$work/package/generated.h" &&
cmp "$work/source/data/nested/fixture" \
    "$work/package/data/nested/fixture" &&
cmp "$work/data/generated" "$work/package/data/generated" || {
    echo 'not ok 1 - test distribution selected incorrect file contents'
    exit 0
}
# Only exercise manifest emission at scale; do not copy thousands of files.
awk -v root="$src_root" 'BEGIN {
    print "include " root "/build-aux/test-dist.am"
    print "inventory:\n\t@: >dist-files.list"
    printf "\t$(call sixel_write_dist_files,"
    name = sprintf("%0160d", 0); gsub(/0/, "x", name)
    for (i = 0; i < 2048; i++) printf "sub/%s.c ", name
    print ")"
}' > "$work/Makefile"
"$make_bin" -s -C "$work" inventory || {
    echo 'not ok 1 - large distribution manifest failed'
    exit 0
}
awk 'BEGIN { name = sprintf("%0160d", 0); gsub(/0/, "x", name) }
    $0 != "sub/" name ".c" { bad = 1 }
    END { exit (bad || NR != 2048) }
' "$work/dist-files.list" || {
    echo 'not ok 1 - large distribution manifest differs'
    exit 0
}
echo 'ok 1 - bounded test distribution manifest'
exit 0

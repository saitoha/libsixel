#!/bin/sh
# Exercise the production installcheck recipe with a large make inventory.
# Policy: docs/build/test-harness.md
set -eux
echo '1..1'
set -v

src_root=$1
make_bin=${SIXEL_STATICCHECK_MAKE:-}
test -n "$make_bin" || make_bin=$(command -v gmake || command -v make)
tmpdir=$(mktemp -d "${TMPDIR:-/tmp}/libsixel-installcheck-XXXXXX")
trap 'rm -rf "$tmpdir"' EXIT HUP INT TERM

# Keep the inventory in make. It exceeds Linux single-argument limits while
# requiring no per-test files or compilation. Extract the real selection and
# launch rules so a change to either process boundary is exercised here.
awk '
BEGIN {
    for (i = 1; i <= 2048; ++i)
        printf "TESTS += suite/%0160d.t\n", i
    print "TESTS += extension/quicklook-installed/0001_probe.t"
    print "SIXEL_INSTALL_BINDIR = ."
    print "SIXEL_INSTALL_LIBDIR = ."
    print "abs_top_builddir = ."
}
# Discovery has its own tests; retain only the installed-selection rule.
/^override TESTS :=/ && /SIXEL_INSTALLCHECK_GENERIC/ { print }
/^installcheck-local:/ { recipe = 1 }
recipe && /^# Override check-TESTS/ { recipe = 0 }
recipe { gsub(/@EXEEXT@/, ""); print }
END {
    print "check-TESTS:"
    print "\t@test \"$(words $(TESTS))\" = \"$(expected)\""
    print "\t@test -z \"$(filter extension/quicklook-installed/%,$(TESTS))\""
    print "\t@test \"$(SIXEL_INSTALLCHECK)\" = 1"
    print "\t@test \"$$SIXEL_INSTALLCHECK_IMG2SIXEL_PATH\" = ./img2sixel"
}
' "$src_root/tests/Makefile.am" > "$tmpdir/Makefile"

status=0
"$make_bin" -s -C "$tmpdir" installcheck-local SIXEL_INSTALLCHECK=1 expected=2048 >&2 || status=1
"$make_bin" -s -C "$tmpdir" installcheck-local SIXEL_INSTALLCHECK=1 expected=1 \
    TESTS='selected.t extension/quicklook-installed/0001_probe.t' >&2 || status=1
"$make_bin" -s -C "$tmpdir" installcheck-local SIXEL_INSTALLCHECK=1 expected=unexpected \
    TESTS='extension/quicklook-installed/0001_probe.t' >&2 || status=1
"$make_bin" -s -C "$tmpdir" installcheck-local SIXEL_INSTALLCHECK=1 expected=unexpected \
    TESTS='' >&2 || status=1
"$make_bin" -s -C "$tmpdir" installcheck-local SIXEL_INSTALLCHECK=0 \
    expected=unexpected >&2 || status=1
test "$status" -eq 0 || {
    echo 'not ok 1 - installed test selection must remain bounded and exact'
    exit 0
}
echo 'ok 1 - installed test selection remains bounded and exact'
exit 0

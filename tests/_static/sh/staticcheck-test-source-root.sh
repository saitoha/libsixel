#!/bin/sh
# Policy: docs/build/test-harness.md
# Policy: docs/testing/staticcheck.md
# Execute the Makefile target selection with distinct POSIX/Windows roots.
set -eux
echo '1..1'
set -v
make_bin=${SIXEL_STATICCHECK_MAKE:-}
test -n "$make_bin" || make_bin=$(command -v gmake || command -v make)
# Keep the production make expansion intact: shell case parentheses must not
# prematurely close a make function. Replace only pwd with two distinct roots.
# Dollar signs in the injected function must survive make before sh sees them.
# shellcheck disable=SC2016
selection=$(awk '
/^SIXEL_TEST_TOP_SRCDIR := / {
    gsub(/@host_os@/, "$(target)")
    sub(/shell /, "shell pwd() { if test -n \"$$1\"; then printf %s D:/workspace/source; else printf %s /d/workspace/source; fi; }; ")
    print
}' "$1/tests/Makefile.am")
test -n "$selection" || {
    echo 'not ok 1 - test source root selection is present'
    exit 0
}
for specimen in \
    'mingw32|D:/workspace/source' \
    'windows-msvc|D:/workspace/source' \
    'msvc|D:/workspace/source' \
    'cygwin|/d/workspace/source' \
    'msys|/d/workspace/source' \
    'linux-gnu|/d/workspace/source'; do
    actual=$("$make_bin" -s --no-print-directory -f - "target=${specimen%%|*}" <<MAKE
abs_top_srcdir = .
$selection
check:
	@printf '%s\n' '\$(SIXEL_TEST_TOP_SRCDIR)'
MAKE
    ) || {
        echo 'not ok 1 - test source root selection failed'
        exit 0
    }
    test "$actual" = "${specimen#*|}" || {
        echo "not ok 1 - wrong test source root for ${specimen%%|*}"
        exit 0
    }
done
echo 'ok 1 - test source roots follow the target path syntax'

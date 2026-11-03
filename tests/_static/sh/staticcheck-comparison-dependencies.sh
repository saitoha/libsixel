#!/bin/sh
# Comparison must remain available without installing diffutils.
# Policy: docs/testing/staticcheck.md
set -eux
echo '1..1'
set -v
src_root=$1
awk '
    /^[[:space:]]*#/ { next }
    /(^|[^[:alnum:]_-])diffutils([^[:alnum:]_-]|$)/ {
        print "# " FILENAME ":" FNR ": diffutils is not a required CI package"
        failed = 1
    }
    END { exit failed }
' "$src_root"/.github/workflows/*.yml || {
    echo 'not ok 1 - CI uses the comparison fallback without diffutils'
    exit 0
}
awk '
    FILENAME ~ /configure.ac$/ && /^SIXEL_COMPARE_SETUP$/ { setup = 1 }
    FILENAME ~ /configure.ac$/ && /^SIXEL_COMPARE_BUILD$/ { native = 1 }
    FILENAME ~ /sixel_compare.m4$/ && /build-aux\/compare/ { bootstrap = 1 }
    FILENAME ~ /tools\/Makefile.am$/ && /\$\(CC_FOR_BUILD\)/ { compiler = 1 }
    FILENAME ~ /compare\/meson.build$/ && /native: true/ { meson = 1 }
    FILENAME ~ /lso-tap-driver.sh.in$/ && /PATH=.*build-aux\/compare/ { tap = 1 }
    FILENAME ~ /staticcheck-suite.sh$/ && /PATH=.*build-aux\/compare/ { source = 1 }
    FILENAME ~ /tests\/meson.build$/ && /build-aux\/compare/ { tests = 1 }
    END { exit !(setup && native && bootstrap && compiler && meson && tap && source && tests) }
' "$src_root/configure.ac" "$src_root/m4/sixel_compare.m4" \
    "$src_root/tools/Makefile.am" "$src_root/tools/compare/meson.build" \
    "$src_root/build-aux/lso-tap-driver.sh.in" \
    "$src_root/tests/_static/sh/staticcheck-suite.sh" \
    "$src_root/tests/meson.build" || {
    echo 'not ok 1 - native and source-only comparison paths are registered'
    exit 0
}
echo 'ok 1 - comparison does not require an external package'
exit 0

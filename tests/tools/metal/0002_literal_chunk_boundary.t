#!/bin/sh
# Policy: docs/build/support-scripts.md
# A source newline crossing the literal limit starts a second array element.
set -eux
echo '1..1'
set -v
actual=$(printf '%1024s\n' '' |
    sh "$TOP_SRCDIR/tools/embed-metal.sh" - -) || {
    echo 'not ok 1 - Metal literal generation failed'
    exit 0
}
case "$actual" in
    *'",'*'",'*'",'*)
        echo 'not ok 1 - the source was split into more than two literals' ;;
    *'",
"\n",')
        echo 'ok 1 - the source newline starts a second C literal' ;;
    *)
        echo 'not ok 1 - the source newline crossed the C literal limit' ;;
esac
exit 0

#!/bin/sh
# Policy: docs/build/support-scripts.md
# Policy: docs/misc/platforms/openvms.md
# Coverage: OV-26
# A source newline crossing the literal limit starts a second array element.
set -eux
echo '1..1'
set -v
# The finite fixture closes at EOF even on GNV's record-oriented streams.
actual=$(sh "$TOP_SRCDIR/tools/embed-metal.sh" \
    "$TOP_SRCDIR/tests/tools/metal/data/chunk-boundary.metal" -) || {
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

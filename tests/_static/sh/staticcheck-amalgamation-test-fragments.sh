#!/bin/sh
# Keep private Objective-C injection bodies out of the common C preamble.
# Policy: docs/build/autotools.md
set -eux
echo '1..1'
set -v

src_root=$1
fixture=$(mktemp -d "${TMPDIR:-/tmp}/libsixel-amalgamation-fragments-XXXXXX")
trap 'rm -rf "$fixture"' EXIT HUP INT TERM
private_root=$src_root/tests/extension/quicklook-lifecycle
headers=$(awk '
/^#define SIXEL_USER_(PROBE|MUTATOR) / {
    name = $3
    gsub(/"/, "", name)
    print name
    count++
}
END { exit count == 2 ? 0 : 1 }
' "$private_root/private-driver.m") || {
    echo 'not ok 1 - private injection headers could not be resolved'
    exit 0
}

# A tiny on-disk input is necessary to exercise the production generator.
# Copy only the two real fragments, not the repository or its test corpus.
mkdir "$fixture/include" "$fixture/tests"
: > "$fixture/config.h"
printf '%s\n' '#define AMALGAMATION_PUBLIC_CONTROL 1' \
    > "$fixture/include/sixel.h"
for header in $headers; do
    test "${header%.inc.h}" != "$header" || {
        echo 'not ok 1 - private injection bodies must be include fragments'
        exit 0
    }
    cat "$private_root/$header" > "$fixture/tests/$header"
done
sh "$src_root/tools/gen-amalgamation.sh" "$fixture/sixel.c" \
    "$fixture" "$fixture" >&2 || {
    echo 'not ok 1 - amalgamation generation failed'
    exit 0
}
awk '
/^#define AMALGAMATION_PUBLIC_CONTROL 1$/ { public_header = 1 }
/^#line .*\/tests\// { private_header = 1 }
END { exit public_header && !private_header ? 0 : 1 }
' "$fixture/sixel.c" || {
    echo 'not ok 1 - private test bodies leaked into the common preamble'
    exit 0
}
echo 'ok 1 - private test fragments stay outside the common C preamble'
exit 0

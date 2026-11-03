#!/bin/sh
# Generated runner selections must not feed back into the common amalgamation.
# Policy: docs/build/autotools.md
# Policy: docs/testing/staticcheck.md
set -eux
echo '1..1'
set -v
src_root=$1
fixture=$(mktemp -d "${TMPDIR:-/tmp}/sixel-amalgamation-inputs-XXXXXX")
trap 'rm -rf "$fixture"' EXIT HUP INT TERM
# A tiny source tree exercises both the initial and incremental generation.
mkdir "$fixture/tests" "$fixture/include"
: > "$fixture/config.h"
printf '%s\n' '#define PUBLIC_CONTROL 1' > "$fixture/include/sixel.h"
printf '%s\n' 'int source_control;' > "$fixture/tests/0001_control.c"
sh "$src_root/tools/gen-amalgamation.sh" "$fixture/before.c" \
    "$fixture" "$fixture" >&2 || {
    echo 'not ok 1 - initial amalgamation generation'
    exit 0
}
printf '%s\n' '#define BUILD_TEST_0001_CONTROL 1' \
    > "$fixture/tests/test-runner-amalgamation-defines.h"
printf '%s\n' '#include "test-runner-amalgamation-defines.h"' \
    '#include "../amalgamation/sixel.c"' \
    > "$fixture/tests/test-runner-amalgamation.c"
sh "$src_root/tools/gen-amalgamation.sh" "$fixture/after.c" \
    "$fixture" "$fixture" >&2 || {
    echo 'not ok 1 - incremental amalgamation generation'
    exit 0
}
cmp -s "$fixture/before.c" "$fixture/after.c" || {
    echo 'not ok 1 - generated runner inputs changed the amalgamation'
    exit 0
}
echo 'ok 1 - generated runner inputs do not change the amalgamation'
exit 0

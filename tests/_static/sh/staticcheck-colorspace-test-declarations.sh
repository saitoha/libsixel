#!/bin/sh
# Read ordinary declarations before a private colorspace copy renames them.
# Policy: docs/build/test-harness.md
# Meson graph: docs/build/meson.md
# Policy: docs/testing/staticcheck.md
set -eux
echo '1..1'
set -v
awk '
function ordered(header, alias, body) {
    return header > 0 && header < alias && alias < body
}
BEGIN {
    if (!ordered(1, 2, 3) || ordered(0, 2, 3) || ordered(3, 2, 4)) {
        failed = 1
        exit 1
    }
}
/^#include "src\/colorspace.h"/ { header = NR }
/^#define sixel_color_convert / { alias = NR }
/^#include "src\/colorspace.c"/ { body = NR }
END { exit failed || !ordered(header, alias, body) }
' "$1/tests/_support/processing/colorspace/public_float_fault.c" || {
    echo 'not ok 1 - private colorspace copies preserve ordinary declarations'
    exit 0
}
echo 'ok 1 - colorspace declarations precede private function aliases'
exit 0

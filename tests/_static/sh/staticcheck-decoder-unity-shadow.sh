#!/bin/sh
# MSVC reports C4459 when a local fixture shadows one from another test.
set -eux
echo '1..1'
set -v

src_root=$1
build_root=$2
cc_bin=${CC:-cc}

printf '%s\n' 'int main(void) { return 0; }' |
    "$cc_bin" -std=c99 -Werror=shadow -x c -fsyntax-only - 2>/dev/null || {
    echo 'ok 1 - decoder unity fixtures # SKIP compiler lacks -Werror=shadow'
    exit 0
}

printf '%s\n' '#include <tests/decoder/parser/0119_token_fast_lane.c>' \
    '#include <tests/decoder/parser/0123_final_pass_alpha.c>' |
    "$cc_bin" -std=c99 -Werror=shadow -I"$src_root" -I"$build_root/include" \
        -x c -fsyntax-only - || {
    echo 'not ok 1 - decoder unity fixtures compile without shadowing'
    exit 0
}

echo 'ok 1 - decoder unity fixtures compile without shadowing'
exit 0

#!/bin/sh
# Policy: docs/build/support-scripts.md
# Compile one real shader as embedded C, independently of the Metal SDK.
set -eux
echo '1..1'
set -v
src_root=$1
cc_bin=${CC:-cc}
test -f "$src_root/src/lookup-policy-blockcull-gpu.metal" || {
    echo 'not ok 1 - the Metal literal fixture is missing'
    exit 0
}
command -v "$cc_bin" >/dev/null 2>&1 || {
    echo 'ok 1 # SKIP C compiler not found'
    exit 0
}
# This 7 KiB program exceeds C99's minimum literal support. Compile the
# generator output so the check observes the actual build representation.
{
    printf '%s\n' 'static char const * const source[] = {'
    awk -f "$src_root/tools/embed-metal.awk" \
        "$src_root/src/lookup-policy-blockcull-gpu.metal"
    printf '%s\n' '};'
} | "$cc_bin" -x c -std=c99 -pedantic-errors -Werror=overlength-strings \
        -fsyntax-only - || {
    echo 'not ok 1 - embedded Metal source literals exceed the C99 limit'
    exit 0
}
echo 'ok 1 - embedded Metal source literals fit the C99 limit'
exit 0

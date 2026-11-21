#!/bin/sh
# The decoder SIMD and parallel paths use compiler-neutral operations.
# Policy: docs/decoder/pipeline/README.md
# Policy: docs/testing/staticcheck.md
set -eux
echo '1..1'
set -v
awk '
function compiler_builtin(s) {
    return s ~ /__builtin_[[:alnum:]_]+[[:space:]]*\(/
}
compiler_builtin($0) { print FILENAME ":" FNR ": " $0; bad=1 }
END {
    if (!compiler_builtin("x = __builtin_clzll(word);")) exit 1
    if (!compiler_builtin("x = __builtin_ctz(mask);")) exit 1
    if (compiler_builtin("x = vmaxvq_u8(positions);")) exit 1
    exit bad
}' "$1/src/decoder-simd.h" "$1/src/decoder-parallel.c" || {
    echo 'not ok 1 - decoder SIMD or parallel path contains a GNU-only builtin'
    exit 0
}
echo 'ok 1 - decoder SIMD and parallel paths use compiler-neutral operations'
exit 0

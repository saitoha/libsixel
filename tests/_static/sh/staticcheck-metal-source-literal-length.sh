#!/bin/sh
# Emit TAP for the embedded Metal source C99 literal-length limit.

set -eu

echo "1..1"

src_root=$1
source_file=$src_root/src/gpu-palette-metal.m
cc_bin=${CC:-cc}

test -f "$source_file" || {
    echo "not ok 1 - embedded Metal source literals fit the C99 limit"
    echo "# missing file: $source_file"
    exit 1
}

command -v "$cc_bin" >/dev/null 2>&1 || {
    echo "ok 1 # SKIP C compiler not found: $cc_bin"
    exit 0
}

# Compile only the literal array so this check remains independent of Metal
# headers and adds one small compiler frontend invocation to staticcheck.
awk '
BEGIN {
    print "#include <stddef.h>"
}
/^static char const \* const g_sixel_gpu_metal_source_chunks\[\] = \{/ {
    in_source = 1
}
in_source {
    print
}
in_source && /^};$/ {
    found = 1
    exit
}
END {
    exit found ? 0 : 1
}
' "$source_file" |
    "$cc_bin" -x c -std=c99 -Werror=overlength-strings \
        -fsyntax-only - || {
    echo "not ok 1 - embedded Metal source literals fit the C99 limit"
    exit 1
}

echo "ok 1 - embedded Metal source literals fit the C99 limit"

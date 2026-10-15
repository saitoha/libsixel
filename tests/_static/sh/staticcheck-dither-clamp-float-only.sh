#!/bin/sh
# Emit TAP for the direction of the dither clamp-policy dependency.
# Policy: docs/encoder/palette-application/clamp-policy.md
#
# The clamp policy bounds error feedback in float coordinates that may leave
# the byte cube.  It is consumed by float diffusion only and must never
# choose the arithmetic: byte diffusion has no clamp policy, and the
# palette-application planner in src/dither.c must not read the policy when
# it decides between byte and float work.

set -eu

src_root=${1:-}
src_dir=$src_root/src

echo "1..2"

if test -z "$src_root" || test ! -d "$src_dir"; then
    echo "not ok 1 - byte dither kernels do not consult the clamp policy"
    echo "# src directory not found: $src_dir"
    echo "not ok 2 - dither planner reads the clamp policy only to store it"
    echo "# src directory not found: $src_dir"
    exit 1
fi

failed=0

# Every byte kernel and byte policy entry point is named *_8bit.  Report any
# clamp-policy field or float limiter call inside those function bodies.
# shellcheck disable=SC2016
matches=$(find "$src_dir" -maxdepth 1 -type f -name 'dither-policy-*.c' \
    -print0 | xargs -0 awk '
/^[A-Za-z_][A-Za-z0-9_]*\(/ {
    name = $0
    sub(/\(.*/, "", name)
    in_byte = name ~ /_8bit$/
    count += in_byte
}
in_byte && /clamp_policy|clamp_threshold|sixel_dither_clamp_/ {
    print FILENAME ":" FNR ":" name ":" $0
    found = 1
}
/^}/ {
    in_byte = 0
}
END {
    if (count == 0) {
        print "no *_8bit dither functions were found"
        found = 1
    }
    exit found ? 1 : 0
}
' || :)

if test -n "$matches"; then
    echo "not ok 1 - byte dither kernels do not consult the clamp policy"
    printf '%s\n' "$matches" | sed 's/^/# /'
    failed=1
else
    echo "ok 1 - byte dither kernels do not consult the clamp policy"
fi

# src/dither.c may initialize the policy and store it through its setter.
# Any other read could again let the policy select float arithmetic.
# shellcheck disable=SC2016
matches=$(awk '
/^[A-Za-z_][A-Za-z0-9_]*\(/ {
    name = $0
    sub(/\(.*/, "", name)
}
/clamp_policy|clamp_threshold|sixel_dither_clamp_|SIXEL_DITHER_CLAMP_/ {
    if (name != "sixel_dither_new" &&
            name != "sixel_dither_set_clamp_policy") {
        print FILENAME ":" FNR ":" name ":" $0
    }
}
' "$src_dir/dither.c")

if test -n "$matches"; then
    echo "not ok 2 - dither planner reads the clamp policy only to store it"
    printf '%s\n' "$matches" | sed 's/^/# /'
    failed=1
else
    echo "ok 2 - dither planner reads the clamp policy only to store it"
fi

exit "$failed"

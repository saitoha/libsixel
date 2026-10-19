#!/bin/sh
# Keep clamp configuration independent of arithmetic precision selection.
# Policy: docs/encoder/palette-application/clamp-policy.md
set -eux
src_root=${1:?source root required}
echo "1..1"
set -v

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
' "$src_root/src/dither.c")

test -z "$matches" || {
    echo "not ok 1 - dither planner reads the clamp policy only to store it"
    printf '%s\n' "$matches" | sed 's/^/# /'
    exit 0
}
echo "ok 1 - dither planner reads the clamp policy only to store it"

exit 0

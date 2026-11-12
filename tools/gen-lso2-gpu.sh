#!/bin/sh
# Generate rational sender coefficients and gain knots from one editable key.
# AWK field references are intentionally not expanded by the shell.
# shellcheck disable=SC2016
set -eu
: "${AWK:=awk}"
# Amalgamation hoists this header outside the Objective-C source guard;
# only the GPU backends (Metal, Vulkan) consume these tables.
printf '%s\n' '#if defined(HAVE_METAL) || defined(HAVE_VULKAN)'
"$AWK" -v NAME=lso2_gpu_ratios -f "${0%/*}/gen_varcoefs.awk" "$1" |
    "$AWK" '
    /^   \{/ {
        printf "    { %s %s %s %s\n      %s %s %s },\n", \
            $2, $3, $4, $5, $6, $7, $8
        next
    }
    { print }
    '
"$AWK" '
    BEGIN { print "static double const lso2_gpu_gain_knots[][2] = {" }
    $1 == "gain" { printf "    { %s, %s },\n", $2, $3 }
    END { print "};" }
    ' "$1"
printf '%s\n' '#endif /* HAVE_METAL || HAVE_VULKAN */'

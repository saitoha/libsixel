#!/bin/sh
# Generate rational sender coefficients and gain knots from one editable key.
# AWK field references are intentionally not expanded by the shell.
# shellcheck disable=SC2016
set -eu
: "${AWK:=awk}"
"$AWK" -v NAME=phased_ratios -f "${0%/*}/gen_varcoefs.awk" "$1" |
    "$AWK" '
    /^   \{/ {
        printf "    { %s %s %s %s\n      %s %s %s },\n", \
            $2, $3, $4, $5, $6, $7, $8
        next
    }
    { print }
    '
"$AWK" '
    BEGIN { print "static double const phased_gain_knots[][2] = {" }
    $1 == "gain" { printf "    { %s, %s },\n", $2, $3 }
    END { print "};" }
    ' "$1"

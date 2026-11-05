#!/bin/sh
# SPDX-License-Identifier: MIT
#
# Score every method of the dithering tutorial on one picture and palette.
#
#   LSQA=path/to/lsqa ./score.sh PICTURE.ppm PALETTE.ppm [DIR]
#
# DIR holds the programs (default: this directory). Add your own method to
# the list below, in the form "name:step:option".

set -u

if [ $# -lt 2 ]; then
    echo "usage: LSQA=path/to/lsqa $0 PICTURE.ppm PALETTE.ppm [DIR]" >&2
    exit 2
fi
picture=$1
palette=$2
here=$(cd "$(dirname "$0")" && pwd)
bin=${3:-$here}
lsqa=${LSQA:-lsqa}
codec="$here/../../sixel-codec/code/step2"
output=$(mktemp "${TMPDIR:-/tmp}/dither-score.XXXXXX") || exit 1
trap 'rm -f "$output"' EXIT INT TERM

methods="nearest:step1: random:step2:random bayer:step2: fs:step3:
serpentine:step4:serpentine atkinson:step4:atkinson jajuni:step4:jajuni"
metrics="MS-SSIM GMSD DE00_region_mean HighFreqRatio_delta StripeScore_rel
BandingIndex_grad_rel"

printf '%-11s' method
for metric in $metrics; do
    printf ' %s' "$metric"
done
[ -x "$codec" ] && printf ' bytes'
printf '\n'

for entry in $methods; do
    name=${entry%%:*}
    rest=${entry#*:}
    step=${rest%%:*}
    option=${rest#*:}
    # shellcheck disable=SC2086 # an empty option is no argument
    "$bin/$step" "$picture" "$palette" $option >"$output" || exit 1
    printf '%-11s' "$name"
    for metric in $metrics; do
        printf ' %s' "$("$lsqa" -m "$metric" "$picture" "$output")"
    done
    [ -x "$codec" ] && printf ' %s' "$("$codec" encode "$output" | wc -c)"
    printf '\n'
done

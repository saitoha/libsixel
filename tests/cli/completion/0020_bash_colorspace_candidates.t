#!/bin/sh
# Check the actual Bash candidates for each encoder color-space selector.
set -eux
printf '1..1\n'
set -v

command -v bash >/dev/null || {
    printf '\n%s\n' 'ok 1 # SKIP bash unavailable'
    exit 0
}
bash --noprofile --norc -c '
    img2sixel() { :; }
    _init_completion() { prev=$option; cur=; }
    source "$1/converters/shell-completion/bash/img2sixel" || exit 1
    geometry="auto icc: srgb srgb-linear oklab lab-d65 din99d"
    option=-W
    _img2sixel
    test "${COMPREPLY[*]}" = "$geometry" || exit 1
    option=-X
    _img2sixel
    test "${COMPREPLY[*]}" = "$geometry" || exit 1
    option=-U
    _img2sixel
    test "${COMPREPLY[*]}" = \
        "srgb srgb-linear smpte-c display-p3 a98-rgb dci-p3 icc auto"
' bash "$TOP_SRCDIR" || {
    printf '\n%s\n' 'not ok 1 - Bash color-space candidates'
    exit 0
}
printf '\n%s\n' 'ok 1 - Bash color-space candidates use canonical names'
exit 0

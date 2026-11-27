#!/bin/sh
# Check the actual Zsh candidates supplied to the completion system.
set -eux
printf '1..1\n'
set -v

command -v zsh >/dev/null || {
    printf '\n%s\n' 'ok 1 # SKIP zsh unavailable'
    exit 0
}
zsh -f -c '
    _arguments() { return 0; }
    _values() {
        shift
        candidates=("${@%%\[*}")
    }
    source "$1/converters/shell-completion/zsh/_img2sixel" || exit 1
    geometry="auto icc: srgb srgb-linear oklab lab-d65 din99d"
    _workingcolorspace
    test "${(j: :)candidates}" = "$geometry" || exit 1
    _clusteringcolorspace
    test "${(j: :)candidates}" = "$geometry" || exit 1
    _outputcolorspace
    test "${(j: :)candidates}" = \
        "srgb srgb-linear smpte-c display-p3 a98-rgb dci-p3 icc auto"
' zsh "$TOP_SRCDIR" || {
    printf '\n%s\n' 'not ok 1 - Zsh color-space candidates'
    exit 0
}
printf '\n%s\n' 'ok 1 - Zsh color-space candidates use canonical names'
exit 0

#!/bin/sh
# Check the replacement text when bash-completion joins a colon-delimited word.
set -eux
echo '1..1'
set -v

command -v bash >/dev/null || {
    printf '\n%s\n' 'ok 1 # SKIP bash unavailable'
    exit 0
}
bash --noprofile --norc -c '
    img2sixel() { :; }
    profile=$1/tests/data/color-numeric/display-p3.icc
    _init_completion() { prev=-X; cur=icc:${profile%?}; }
    _filedir() {
        test "$cur" = "${profile%?}" || exit 1
        COMPREPLY=("$profile")
    }
    source "$1/converters/shell-completion/bash/img2sixel"
    COMP_WORDBREAKS=" :"
    _img2sixel
    test "${#COMPREPLY[@]}" = 1 && test "${COMPREPLY[0]}" = "$profile" || exit 1
    COMP_WORDBREAKS=" "
    _img2sixel
    test "${#COMPREPLY[@]}" = 1 && test "${COMPREPLY[0]}" = "icc:$profile"
' bash "$TOP_SRCDIR" || {
    printf '\n%s\n' 'not ok 1 - Bash ICC completion duplicates the prefix'
    exit 0
}
printf '\n%s\n' 'ok 1 - Bash ICC completion replaces only the path suffix'
exit 0

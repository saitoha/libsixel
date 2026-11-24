#!/bin/sh
# TAP test verifying invalid decoder arguments surface descriptive errors.

set -eux

test "${HAVE_SIXEL2PNG-}" = 1 || {
    printf "1..0 # SKIP sixel2png is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v

input_path="${TOP_SRCDIR}/images/map8.six"

msg=$(set +xv; ${SIXEL_RUNTIME-} "${SIXEL2PNG_PATH}" --similarity=invalid -o/dev/null "${input_path}" \
    2>&1) && {
    printf '\n%s\n' "not ok 1 - invalid similarity should fail"
    exit 0
}

case "${msg}" in
    *[Ss][Ii][Mm][Ii][Ll][Aa][Rr][Ii][Tt][Yy]*|*[Ss][Ii][Xx][Ee][Ll]_[Bb][Aa][Dd]_[Aa][Rr][Gg][Uu][Mm][Ee][Nn][Tt]*)
        printf '\n%s\n' "ok 1 - invalid similarity reported"
        exit 0
        ;;
    *)
        ;;
esac

printf '\n%s\n' "not ok 1 - error message missing similarity hint"
printf '%s\n' '--- stderr ---' >&2
printf '%s\n' "${msg}" >&2
exit 0

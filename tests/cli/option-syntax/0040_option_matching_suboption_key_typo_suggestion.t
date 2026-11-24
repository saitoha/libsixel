#!/bin/sh
# TAP test verifying suboption key typos suggest the canonical key.
# Policy: docs/cli/correction-suggestions.md

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

msg=''
result=0

set +x
msg=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Qkmeans:inittpye=pca \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" \
    -o/dev/null 2>&1) || result=$?
set -x

test "${result}" -eq 2 || {
    printf '\n%s\n' "not ok 1 - suboption key typo exit status mismatch"
    exit 0
}

case "${msg}" in
    *'unknown suboption key "inittpye".'*'Did you mean: inittype?'*)
        ;;
    *)
        printf '\n%s\n' "not ok 1 - suboption key typo suggestion mismatch"
        printf '%s\n' '--- stderr ---' >&2
        printf '%s\n' "${msg}" >&2
        exit 0
        ;;
esac

printf '\n%s\n' "ok 1 - suboption key typo suggests the canonical key"
exit 0

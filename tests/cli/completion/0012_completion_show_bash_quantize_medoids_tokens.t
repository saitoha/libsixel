#!/bin/sh
# TAP test verifying bash completion output matches source script.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

echo "1..1"
set -v
set +x

status=0
msg=''
actual_cksum=''
expected_cksum=''
completion_file="${TOP_SRCDIR}/converters/shell-completion/bash/img2sixel"

test -f "${completion_file}" || {
    printf '\n%s\n' "not ok 1 - missing bash completion source script"
    exit 0
}

msg=$(
    set +xv
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        --env IMG2SIXEL_COMPLETION_DIR="${TOP_SRCDIR}/converters/shell-completion" \
        -1 bash
    printf '__LIBSIXEL_BASH_COMPLETION_END__'
) || status=$?

test "${status}" = 0 || {
    printf '\n%s\n' "not ok 1 - bash completion output failed"
    exit 0
}

case "${msg}" in
    *__LIBSIXEL_BASH_COMPLETION_END__)
        ;;
    *)
        printf '\n%s\n' "not ok 1 - bash completion output marker missing"
        exit 0
        ;;
esac

msg=${msg%__LIBSIXEL_BASH_COMPLETION_END__}
# Normalize line endings so Windows text-mode stdout does not cause a
# false mismatch against repository files stored with LF endings.
actual_cksum=$(printf '%s' "${msg}" | tr -d '\r' | "${CKSUM:-cksum}")
expected_cksum=$(tr -d '\r' < "${completion_file}" | "${CKSUM:-cksum}")

test "${actual_cksum}" = "${expected_cksum}" || {
    printf '\n%s\n' "not ok 1 - bash completion output diverges from source script"
    echo "# expected: ${expected_cksum}"
    echo "# actual:   ${actual_cksum}"
    exit 0
}

printf '\n%s\n' "ok 1 - bash completion output matches source script"
exit 0

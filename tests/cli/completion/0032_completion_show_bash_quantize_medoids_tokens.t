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
    echo "not ok" 1 - "missing bash completion source script"
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
    echo "not ok" 1 - "bash completion output failed"
    exit 0
}

case "${msg}" in
    *__LIBSIXEL_BASH_COMPLETION_END__)
        ;;
    *)
        echo "not ok" 1 - "bash completion output marker missing"
        exit 0
        ;;
esac

msg=${msg%__LIBSIXEL_BASH_COMPLETION_END__}
actual_cksum=$(printf '%s' "${msg}" | cksum)
expected_cksum=$(cksum < "${completion_file}")

test "${actual_cksum}" = "${expected_cksum}" || {
    echo "not ok" 1 - "bash completion output diverges from source script"
    echo "# expected: ${expected_cksum}"
    echo "# actual:   ${actual_cksum}"
    exit 0
}

echo "ok" 1 - "bash completion output matches source script"
exit 0

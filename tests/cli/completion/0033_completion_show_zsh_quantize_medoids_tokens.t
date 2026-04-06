#!/bin/sh
# TAP test verifying zsh completion output matches source script.

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
completion_file="${TOP_SRCDIR}/converters/shell-completion/zsh/_img2sixel"

test -f "${completion_file}" || {
    echo "not ok" 1 - "missing zsh completion source script"
    exit 0
}

msg=$(
    set +xv
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        --env IMG2SIXEL_COMPLETION_DIR="${TOP_SRCDIR}/converters/shell-completion" \
        -1 zsh
    printf '__LIBSIXEL_ZSH_COMPLETION_END__'
) || status=$?

test "${status}" = 0 || {
    echo "not ok" 1 - "zsh completion output failed"
    exit 0
}

case "${msg}" in
    *__LIBSIXEL_ZSH_COMPLETION_END__)
        ;;
    *)
        echo "not ok" 1 - "zsh completion output marker missing"
        exit 0
        ;;
esac

msg=${msg%__LIBSIXEL_ZSH_COMPLETION_END__}
# Normalize line endings so Windows text-mode stdout does not cause a
# false mismatch against repository files stored with LF endings.
actual_cksum=$(printf '%s' "${msg}" | tr -d '\r' | cksum)
expected_cksum=$(tr -d '\r' < "${completion_file}" | cksum)

test "${actual_cksum}" = "${expected_cksum}" || {
    echo "not ok" 1 - "zsh completion output diverges from source script"
    echo "# expected: ${expected_cksum}"
    echo "# actual:   ${actual_cksum}"
    exit 0
}

echo "ok" 1 - "zsh completion output matches source script"
exit 0

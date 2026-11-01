#!/bin/sh
# TAP test verifying combined bash/zsh completion output.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

echo '1..1'
set -v

completion_output=''

completion_output=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -1 all) || {
    echo "not ok" 1 - "combined completion output failed"
    exit 0
}
: "${completion_output}"

case "${completion_output}" in
    *"# bash completion for img2sixel"*) ;;
    *)
    echo "not ok" 1 - "missing bash completion header in combined output"
    exit 0
    ;;
esac

case "${completion_output}" in
    *"#compdef img2sixel"*) ;;
    *)
    echo "not ok" 1 - "missing zsh completion header in combined output"
    exit 0
    ;;
esac

echo "ok" 1 - "combined completion output available"
exit 0

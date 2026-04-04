#!/bin/sh
# TAP test verifying zsh completion exports kmedoids quantize tokens.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v

msg=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env IMG2SIXEL_COMPLETION_DIR="${TOP_SRCDIR}/converters/shell-completion" -1 zsh) || {
    echo "not ok" 1 - "zsh completion output failed"
    exit 0
}

test "${msg#*kmedoids*}" != "${msg}" || {
    echo "not ok" 1 - "missing kmedoids base candidate in zsh completion"
    exit 0
}

test "${msg#*algo=*}" != "${msg}" || {
    echo "not ok" 1 - "missing kmedoids algo key in zsh completion"
    exit 0
}

test "${msg#*banditpam*}" != "${msg}" || {
    echo "not ok" 1 - "missing kmedoids algo values in zsh completion"
    exit 0
}

test "${msg#*seed=*}" != "${msg}" || {
    echo "not ok" 1 - "missing kmedoids seed key in zsh completion"
    exit 0
}

echo "ok" 1 - "zsh completion includes kmedoids quantize tokens"
exit 0

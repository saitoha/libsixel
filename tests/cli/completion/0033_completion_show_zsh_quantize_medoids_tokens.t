#!/bin/sh
# TAP test verifying zsh completion exports medoids quantize tokens.

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

test "${msg#*medoids*}" != "${msg}" || {
    echo "not ok" 1 - "missing medoids base candidate in zsh completion"
    exit 0
}

test "${msg#*algo=*}" != "${msg}" || {
    echo "not ok" 1 - "missing medoids algo key in zsh completion"
    exit 0
}

test "${msg#*banditpam*}" != "${msg}" || {
    echo "not ok" 1 - "missing medoids algo values in zsh completion"
    exit 0
}

test "${msg#*auto*}" != "${msg}" || {
    echo "not ok" 1 - "missing medoids auto algo value in zsh completion"
    exit 0
}

test "${msg#*seed=*}" != "${msg}" || {
    echo "not ok" 1 - "missing medoids seed key in zsh completion"
    exit 0
}

test "${msg#*iter=*}" != "${msg}" || {
    echo "not ok" 1 - "missing medoids iter key in zsh completion"
    exit 0
}

test "${msg#*sample=*}" != "${msg}" || {
    echo "not ok" 1 - "missing medoids sample key in zsh completion"
    exit 0
}

test "${msg#*clara_trials=*}" != "${msg}" || {
    echo "not ok" 1 - "missing medoids clara_trials key in zsh completion"
    exit 0
}

test "${msg#*clara_sample=*}" != "${msg}" || {
    echo "not ok" 1 - "missing medoids clara_sample key in zsh completion"
    exit 0
}

test "${msg#*clarans_local=*}" != "${msg}" || {
    echo "not ok" 1 - "missing medoids clarans_local key in zsh completion"
    exit 0
}

test "${msg#*clarans_neighbors=*}" != "${msg}" || {
    echo "not ok" 1 - "missing medoids clarans_neighbors key in zsh completion"
    exit 0
}

test "${msg#*bandit_iter=*}" != "${msg}" || {
    echo "not ok" 1 - "missing medoids bandit_iter key in zsh completion"
    exit 0
}

test "${msg#*bandit_candidates=*}" != "${msg}" || {
    echo "not ok" 1 - "missing medoids bandit_candidates key in zsh completion"
    exit 0
}

test "${msg#*bandit_batch=*}" != "${msg}" || {
    echo "not ok" 1 - "missing medoids bandit_batch key in zsh completion"
    exit 0
}

echo "ok" 1 - "zsh completion includes medoids quantize tokens"
exit 0

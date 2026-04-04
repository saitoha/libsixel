#!/bin/sh
# TAP test verifying bash completion exports kmedoids quantize tokens.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v

msg=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env IMG2SIXEL_COMPLETION_DIR="${TOP_SRCDIR}/converters/shell-completion" -1 bash) || {
    echo "not ok" 1 - "bash completion output failed"
    exit 0
}

test "${msg#*kmedoids*}" != "${msg}" || {
    echo "not ok" 1 - "missing kmedoids base candidate in bash completion"
    exit 0
}

test "${msg#*algo=*}" != "${msg}" || {
    echo "not ok" 1 - "missing kmedoids algo key in bash completion"
    exit 0
}

test "${msg#*banditpam*}" != "${msg}" || {
    echo "not ok" 1 - "missing kmedoids algo values in bash completion"
    exit 0
}

test "${msg#*auto*}" != "${msg}" || {
    echo "not ok" 1 - "missing kmedoids auto algo value in bash completion"
    exit 0
}

test "${msg#*seed=*}" != "${msg}" || {
    echo "not ok" 1 - "missing kmedoids seed key in bash completion"
    exit 0
}

test "${msg#*iter=*}" != "${msg}" || {
    echo "not ok" 1 - "missing kmedoids iter key in bash completion"
    exit 0
}

test "${msg#*sample=*}" != "${msg}" || {
    echo "not ok" 1 - "missing kmedoids sample key in bash completion"
    exit 0
}

test "${msg#*clara_trials=*}" != "${msg}" || {
    echo "not ok" 1 - "missing kmedoids clara_trials key in bash completion"
    exit 0
}

test "${msg#*clara_sample=*}" != "${msg}" || {
    echo "not ok" 1 - "missing kmedoids clara_sample key in bash completion"
    exit 0
}

test "${msg#*clarans_local=*}" != "${msg}" || {
    echo "not ok" 1 - "missing kmedoids clarans_local key in bash completion"
    exit 0
}

test "${msg#*clarans_neighbors=*}" != "${msg}" || {
    echo "not ok" 1 - "missing kmedoids clarans_neighbors key in bash completion"
    exit 0
}

test "${msg#*bandit_iter=*}" != "${msg}" || {
    echo "not ok" 1 - "missing kmedoids bandit_iter key in bash completion"
    exit 0
}

test "${msg#*bandit_candidates=*}" != "${msg}" || {
    echo "not ok" 1 - "missing kmedoids bandit_candidates key in bash completion"
    exit 0
}

test "${msg#*bandit_batch=*}" != "${msg}" || {
    echo "not ok" 1 - "missing kmedoids bandit_batch key in bash completion"
    exit 0
}

echo "ok" 1 - "bash completion includes kmedoids quantize tokens"
exit 0

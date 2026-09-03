#!/bin/sh
# Verify sixel2png applies the shared diagnostics policy.

set -eux

test "${HAVE_SIXEL2PNG-}" = 1 || {
    echo "1..0 # SKIP sixel2png is disabled in this build"
    exit 0
}

echo "1..1"
set -v

human_status=0
code_status=0
env_status=0
human_message=$(set +xv; ${SIXEL_RUNTIME-} "${SIXEL2PNG_PATH}" \
    -xhuman:P1 -d k_undithr 2>&1) || human_status=$?
code_message=$(set +xv; ${SIXEL_RUNTIME-} "${SIXEL2PNG_PATH}" \
    -xcode -d k_undithr 2>&1) || code_status=$?
env_message=$(set +xv; ${SIXEL_RUNTIME-} "${SIXEL2PNG_PATH}" \
    --env SIXEL_DIAG_MODE=code -d k_undithr 2>&1) || env_status=$?

test "${human_status}" -eq 255 || {
    echo "not ok" 1 - "human diagnostics exit status mismatch"
    exit 0
}

test "${code_status}" -eq 255 || {
    echo "not ok" 1 - "code diagnostics exit status mismatch"
    exit 0
}

test "${env_status}" -eq 255 || {
    echo "not ok" 1 - "environment diagnostics exit status mismatch"
    exit 0
}

test "${human_message#*valid values:*}" != "${human_message}" || {
    echo "not ok" 1 - "human diagnostics omitted valid values"
    exit 0
}

test "${code_message#*valid values:*}" = "${code_message}" || {
    echo "not ok" 1 - "code diagnostics retained valid values"
    exit 0
}

test "${code_message}" = "${env_message}" || {
    echo "not ok" 1 - "decoder CLI and environment diagnostics differ"
    exit 0
}

echo "ok" 1 - "sixel2png applies shared diagnostics policy"
exit 0

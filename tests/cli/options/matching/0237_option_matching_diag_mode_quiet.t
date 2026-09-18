#!/bin/sh
# Verify quiet code diagnostics suppress the option help body.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

verbose_status=0
quiet_status=0
verbose_message=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env SIXEL_DIAG_MODE=code \
    --env SIXEL_DIAG_MODE_QUIET=0 \
    -d invalidvalue "${TOP_SRCDIR}/tests/data/inputs/small.ppm" \
    -o/dev/null 2>&1) || verbose_status=$?
quiet_message=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env SIXEL_DIAG_MODE=code \
    --env SIXEL_DIAG_MODE_QUIET=1 \
    -d invalidvalue "${TOP_SRCDIR}/tests/data/inputs/small.ppm" \
    -o/dev/null 2>&1) || quiet_status=$?

test "${verbose_status}" -eq 2 || {
    echo "not ok" 1 - "verbose diagnostic exit status mismatch"
    exit 0
}

test "${quiet_status}" -eq 2 || {
    echo "not ok" 1 - "quiet diagnostic exit status mismatch"
    exit 0
}

case "${verbose_message}" in *-d\ DIFFUSION,**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "verbose diagnostic omits option help"
    exit 0
}

case "${quiet_message}" in *-d\ DIFFUSION,**) false ;; *) true ;; esac || {
    echo "not ok" 1 - "quiet diagnostic includes option help"
    exit 0
}

case "${quiet_message}" in *unknown\ option\ base\ value\ \"invalidvalue\".**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "quiet diagnostic detail is missing"
    exit 0
}

echo "ok" 1 - "quiet code diagnostic suppresses option help"
exit 0

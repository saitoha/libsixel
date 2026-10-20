#!/bin/sh
# Verify invalid option value keeps human diagnostics by default.
# Policy: docs/cli/diagnostics.md

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled in this build"
    exit 0
}

echo "1..1"
set -v

unset SIXEL_DIAG_MODE
unset SIXEL_TRACE_TOPIC
status=0
message=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -p invalid 2>&1 >/dev/null) || status=$?

test "${status}" -eq 2 || {
    echo "not ok 1 - option error exit status changed"
    exit 0
}

case "${message}" in *LSX*) false ;; *) true ;; esac || {
    echo "not ok 1 - option error machine header policy mismatch"
    printf '%s\n' "${message}" >&2
    exit 0
}

case "${message}" in *invalid*) true ;; *) false ;; esac || {
    echo "not ok 1 - ordinary option error diagnostic is missing"
    printf '%s\n' "${message}" >&2
    exit 0
}

echo "ok 1 - invalid option value keeps human diagnostics by default"
exit 0

#!/bin/sh
# TAP wrapper for libwebp lossy WebPDecode fault-injection case.

set -eux

test "${HAVE_WEBP-}" = 1 || {
    printf "1..0 # SKIP libwebp loader is unavailable\n"
    exit 0
}


echo "1..1"
set -v

status=0
msg=$(set +xv; ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0035_loader_libwebp_fault_lossy_decode" 2>&1) || status=$?

case "${status}" in
    0)
        echo "ok" 1 - "loader/0035_loader_libwebp_fault_lossy_decode"
        ;;
    *)
        echo "not ok" 1 - "loader/0035_loader_libwebp_fault_lossy_decode"
        printf '%s\n' '--- stderr ---' >&2
        printf '%s\n' "${msg}" >&2
        ;;
esac

exit 0

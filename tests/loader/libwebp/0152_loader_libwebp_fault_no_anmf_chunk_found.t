#!/bin/sh
# TAP wrapper for libwebp no-ANMF fallback fault-injection case.

set -eux

test "${HAVE_WEBP-}" = 1 || {
    printf "1..0 # SKIP libwebp loader is unavailable\n"
    exit 0
}


echo "1..1"
set -v

status=0
msg=$(set +xv; ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0040_loader_libwebp_fault_no_anmf" 2>&1) || status=$?

case "${status}" in
    0)
        echo "ok" 1 - "loader/0040_loader_libwebp_fault_no_anmf"
        ;;
    *)
        echo "not ok" 1 - "loader/0040_loader_libwebp_fault_no_anmf"
        printf '%s\n' '--- stderr ---' >&2
        printf '%s\n' "${msg}" >&2
        ;;
esac

exit 0

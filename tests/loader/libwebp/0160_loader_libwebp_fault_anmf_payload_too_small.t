#!/bin/sh
# TAP wrapper for libwebp ANMF payload-too-small extraction fault case.

set -eux

test "${HAVE_WEBP-}" = 1 || {
    printf "1..0 # SKIP libwebp loader is unavailable\n"
    exit 0
}


echo "1..1"
set -v

status=0
msg=$(set +xv; ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0048_loader_libwebp_fault_anmf_payload_too_small" 2>&1) || status=$?

case "${status}" in
    0)
        echo "ok" 1 - "loader/0048_loader_libwebp_fault_anmf_payload_too_small"
        ;;
    *)
        echo "not ok" 1 - "loader/0048_loader_libwebp_fault_anmf_payload_too_small"
        printf '%s\n' '--- stderr ---' >&2
        printf '%s\n' "${msg}" >&2
        ;;
esac

exit 0

#!/bin/sh
# TAP wrapper for libwebp lossy missing YUV plane fault-injection case.

set -eux

case "${HAVE_WEBP-}" in
    1)
        ;;
    *)
        printf "1..0 # SKIP libwebp loader is unavailable\n"
        exit 0
        ;;
esac

echo "1..1"
set -v

status=0
msg=$(set +xv; ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0036_loader_libwebp_fault_lossy_yuv_plane_missing" 2>&1) || status=$?

case "${status}" in
    0)
        echo "ok" 1 - "loader/0036_loader_libwebp_fault_lossy_yuv_plane_missing"
        ;;
    *)
        echo "not ok" 1 - "loader/0036_loader_libwebp_fault_lossy_yuv_plane_missing"
        printf '%s\n' '--- stderr ---' >&2
        printf '%s\n' "${msg}" >&2
        ;;
esac

exit 0

#!/bin/sh
# Verify spatial async stale fallback.
# Policy: docs/encoder/output/delta-encoding.md
set -eux
echo "1..1"
set -v
test "${SIXEL_ENABLE_THREADS-0}" = 1 || {
    printf '\n%s\n' "ok 1 # SKIP threading is disabled"
    exit 0
}
status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    delta/refresh/0090_spatial_async_stale_fallback || status=$?
test "$status" -ne 77 || {
    printf '\n%s\n' "ok 1 # SKIP required runtime is unavailable"
    exit 0
}
test "$status" -eq 0 || {
    printf '\n%s\n' "not ok 1 - spatial async stale fallback"
    exit 0
}
printf '\n%s\n' "ok 1 - spatial async stale fallback"
exit 0

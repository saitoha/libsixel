#!/bin/sh
# Verify spatial option accepts g2.
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
    delta/refresh/0092_spatial_option_accepts_g2 || status=$?
test "$status" -ne 77 || {
    printf '\n%s\n' "ok 1 # SKIP required runtime is unavailable"
    exit 0
}
test "$status" -eq 0 || {
    printf '\n%s\n' "not ok 1 - spatial option accepts g2"
    exit 0
}
printf '\n%s\n' "ok 1 - spatial option accepts g2"
exit 0

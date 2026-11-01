#!/bin/sh
# palette store short serial d0.
# Policy: docs/misc/platforms/openvms.md
# Coverage: OV-22
# Policy: docs/cli/threading-policy.md
# Build configuration: docs/build/meson.md
set -eux

test "${SIXEL_ENABLE_THREADS-0}" = 1 || {
    echo "1..0 # SKIP thread support disabled"
    exit 0
}

echo "1..1"
set -v

status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    planner/pipeline/0091_palette_auto_store_return 0 0 || status=$?
test "$status" -ne 77 || {
    echo "ok 1 # SKIP thread support disabled"
    exit 0
}
test "$status" -eq 0 || {
    echo "not ok 1 - palette store short serial d0"
    exit 0
}
echo "ok 1 - palette store short serial d0"
exit 0

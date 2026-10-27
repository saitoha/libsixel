#!/bin/sh
# Policy: docs/misc/platforms/openvms.md
# Coverage: OV-22
# A completed palette survives frame probes and is reused on return.
# Policy: docs/cli/threading-policy.md
# Build configuration: docs/build/meson.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    planner/pipeline/0091_palette_auto_store_return || {
    echo "not ok 1 - palette auto store return"
    exit 0
}
echo "ok 1 - palette auto store return"
exit 0

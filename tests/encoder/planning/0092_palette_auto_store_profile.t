#!/bin/sh
# Policy: docs/misc/platforms/openvms.md
# Coverage: OV-22
# A profile transition during frame behavior invalidates retained palettes.
# Policy: docs/cli/threading-policy.md
# Build configuration: docs/build/meson.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    planner/pipeline/0092_palette_auto_store_profile || {
    echo "not ok 1 - palette auto store profile"
    exit 0
}
echo "ok 1 - palette auto store profile"
exit 0

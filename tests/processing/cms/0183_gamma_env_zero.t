#!/bin/sh
# Policy: docs/loader/color-management/README.md
# Policy: docs/cli/design-policy.md
# env-only gamma zero uses the shared scalar registry contract.
set -eux
echo "1..1"
set -v
export SIXEL_LOADER_CMS_COLORSYNC_NATIVE_GAMMA=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" cms/0181_gamma_option 1 0 || {
    echo "not ok 1 - env-only gamma zero"
    exit 0
}
echo "ok 1 - env-only gamma zero"
exit 0

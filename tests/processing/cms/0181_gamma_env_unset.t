#!/bin/sh
# Policy: docs/loader/color-management.md
# Policy: docs/cli/design-policy.md
# env-only gamma unset uses the shared scalar registry contract.
set -eux
echo "1..1"
set -v
unset SIXEL_LOADER_CMS_COLORSYNC_NATIVE_GAMMA
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" cms/0181_gamma_option 0 123 || {
    echo "not ok 1 - env-only gamma unset"
    exit 0
}
echo "ok 1 - env-only gamma unset"
exit 0

#!/bin/sh
# Policy: docs/loader/color-management/README.md
# Policy: docs/cli/design-policy.md
# env-only gamma leading space uses the shared scalar registry contract.
set -eux
echo "1..1"
set -v
export SIXEL_LOADER_CMS_COLORSYNC_NATIVE_GAMMA=' 1'
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" cms/0181_gamma_option -1 123 || {
    echo "not ok 1 - env-only gamma leading space"
    exit 0
}
echo "ok 1 - env-only gamma leading space"
exit 0

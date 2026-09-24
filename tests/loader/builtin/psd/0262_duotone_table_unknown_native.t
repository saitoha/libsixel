#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/psd.md
# Verify the saved Lab table remains usable with an opaque native version.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/psd/0262_duotone_table_unknown_native 1>&2 || {
    echo "not ok 1 - duotone table unknown native"
    exit 0
}
echo "ok 1 - duotone table unknown native"
exit 0

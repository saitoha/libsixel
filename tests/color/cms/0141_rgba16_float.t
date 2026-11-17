#!/bin/sh
# Policy: docs/loader/builtin-cms.md
# Verify rgba16 float with independent expected values.
set -eux
echo '1..1'
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" cms/0141_rgba16_float || {
    echo 'not ok 1 - rgba16_float'
    exit 0
}
echo 'ok 1 - rgba16_float'
exit 0

#!/bin/sh
# Policy: docs/loader/builtin-cms.md
# Verify gray16 float with independent expected values.
set -eux
echo '1..1'
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" cms/0142_gray16_float || {
    echo 'not ok 1 - gray16_float'
    exit 0
}
echo 'ok 1 - gray16_float'
exit 0

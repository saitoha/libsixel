#!/bin/sh
# Policy: docs/loader/builtin-cms.md
# Verify bgra8 float with independent expected values.
set -eux
echo '1..1'
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" cms/0148_bgra8_float || {
    printf '\n%s\n' 'not ok 1 - bgra8_float'
    exit 0
}
printf '\n%s\n' 'ok 1 - bgra8_float'
exit 0

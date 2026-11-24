#!/bin/sh
# Policy: docs/loader/builtin-cms.md
# Verify rgb16 alias with independent expected values.
set -eux
echo '1..1'
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" cms/0140_rgb16_alias || {
    printf '\n%s\n' 'not ok 1 - rgb16_alias'
    exit 0
}
printf '\n%s\n' 'ok 1 - rgb16_alias'
exit 0

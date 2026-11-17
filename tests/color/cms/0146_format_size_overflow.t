#!/bin/sh
# Policy: docs/loader/builtin-cms.md
# Verify format size overflow with independent expected values.
set -eux
echo '1..1'
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" cms/0146_format_size_overflow || {
    echo 'not ok 1 - format_size_overflow'
    exit 0
}
echo 'ok 1 - format_size_overflow'
exit 0

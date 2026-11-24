#!/bin/sh
# Policy: docs/loader/builtin-cms.md
# Verify release, atomic failure and retry after output allocation failure.
set -eux
echo '1..1'
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" cms/0151_adapter_output_allocation || {
    printf '\n%s\n' 'not ok 1 - adapter output allocation'
    exit 0
}
printf '\n%s\n' 'ok 1 - adapter output allocation'
exit 0

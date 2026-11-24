#!/bin/sh
# Policy: docs/loader/builtin-cms.md
# Verify release, atomic failure and retry after input allocation failure.
set -eux
echo '1..1'
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" cms/0150_adapter_input_allocation || {
    printf '\n%s\n' 'not ok 1 - adapter input allocation'
    exit 0
}
printf '\n%s\n' 'ok 1 - adapter input allocation'
exit 0

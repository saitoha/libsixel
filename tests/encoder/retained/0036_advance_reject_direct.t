#!/bin/sh
# Resolved advance cannot publish a direct update.
# Policy: docs/encoder/output/delta-encoding.md
set -eux
echo '1..1'
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "encoder/retained/0036_advance_reject_direct" || {
    echo 'not ok 1 - Resolved advance cannot publish a direct update.'
    exit 0
}
echo 'ok 1 - Resolved advance cannot publish a direct update.'
exit 0

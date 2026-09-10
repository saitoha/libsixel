#!/bin/sh
# Run the scoped suboption registry test.
# Policy: docs/cli/suboptions.md

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "cli/0033_cli_suboption_consumer_scope" || {
    echo "not ok 1 - suboptions honor consumer scope"
    exit 0
}

echo "ok 1 - suboptions honor consumer scope"
exit 0

#!/bin/sh

# Policy: docs/cli/design-policy.md

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "cli/0035_cli_lookup_policy_initial_prefixes" || {
    echo "not ok 1 - registered lookup policy initials"
    exit 0
}

echo "ok 1 - registered lookup policy initials"
exit 0

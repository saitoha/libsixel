#!/bin/sh

# Policy: docs/cli/prefix-matching.md

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "cli/0035_cli_lookup_policy_initial_prefixes" || {
    printf '\n%s\n' "not ok 1 - registered lookup policy initials"
    exit 0
}

printf '\n%s\n' "ok 1 - registered lookup policy initials"
exit 0

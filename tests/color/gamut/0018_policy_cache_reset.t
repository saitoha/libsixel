#!/bin/sh
# Policy: docs/encoder/output/gamut-mapping.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" gamut/0018_policy_cache_reset || {
    printf '\n%s\n' "not ok 1 - policy cache reset"
    exit 0
}
printf '\n%s\n' "ok 1 - policy cache reset"
exit 0

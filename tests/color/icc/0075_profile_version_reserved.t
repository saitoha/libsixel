#!/bin/sh

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0075_profile_version_reserved || {
    printf '\n%s\n' "not ok 1 - nonzero ICC version reserved bytes are malformed"
    exit 0
}

printf '\n%s\n' "ok 1 - nonzero ICC version reserved bytes are malformed"
exit 0

#!/bin/sh

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0067_profile_version_invalid_bcd || {
    echo "not ok 1 - invalid ICC version BCD is reported malformed"
    exit 0
}

echo "ok 1 - invalid ICC version BCD is reported malformed"
exit 0

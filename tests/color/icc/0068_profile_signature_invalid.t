#!/bin/sh

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0068_profile_signature_invalid || {
    echo "not ok 1 - an explicit wrong ICC signature is malformed"
    exit 0
}

echo "ok 1 - an explicit wrong ICC signature is malformed"
exit 0

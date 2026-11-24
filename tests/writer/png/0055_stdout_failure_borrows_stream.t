#!/bin/sh
# Policy: docs/writers/png/README.md
# PNG write failure leaves standard output open.
set -eux

echo "1..1"
set -v

SIXEL_LOG_PATH='' ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    writer/png/0055_stdout_failure_borrows_stream || {
    printf '\n%s\n' "not ok 1 - PNG write failure leaves standard output open"
    exit 0
}
printf '\n%s\n' "ok 1 - PNG write failure leaves standard output open"
exit 0

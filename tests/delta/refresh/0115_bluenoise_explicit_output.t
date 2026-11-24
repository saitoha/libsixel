#!/bin/sh
# Verify bluenoise explicit output.
# Policy: docs/encoder/output/delta-encoding.md
set -eux
echo "1..1"
set -v
status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    delta/refresh/0115_bluenoise_explicit_output >&2 || status=$?
test "$status" -eq 0 || {
    printf '\n%s\n' "not ok 1 - bluenoise explicit output"
    exit 0
}
printf '\n%s\n' "ok 1 - bluenoise explicit output"
exit 0

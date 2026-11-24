#!/bin/sh
# Verify failed output preserves destination.
# Policy: docs/misc/platforms/openvms.md
# Coverage: OV-19
set -eux

echo "1..1"
set -v

set +e
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "progressive/0013_failed_output_preserves_destination" \
    "${TOP_SRCDIR}/images/snake-grayscale.png/output.six"
result=$?
set -e
test "$result" -ne 77 || {
    printf '\n%s\n' "ok 1 - failed output preserves destination # SKIP thread support disabled"
    exit 0
}
test "$result" -eq 0 || {
    printf '\n%s\n' "not ok 1 - failed output preserves destination"
    exit 0
}
printf '\n%s\n' "ok 1 - failed output preserves destination"
exit 0

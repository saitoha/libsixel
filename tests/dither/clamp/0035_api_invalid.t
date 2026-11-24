#!/bin/sh
# Invalid low-level updates preserve both settings.
# Policy: docs/encoder/palette-application/clamp-policy.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "dither/clamp/0035_api_invalid" || {
    printf '\n%s\n' "not ok 1 - api_invalid"
    exit 0
}
printf '\n%s\n' "ok 1 - api_invalid"
exit 0

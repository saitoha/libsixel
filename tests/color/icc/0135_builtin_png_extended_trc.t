#!/bin/sh
# Verify one ICC TRC application and extended linear float32 PNG output.
set -eux
echo '1..1'
set -v

SIXEL_LOADER_CMS_BPC=0 SIXEL_LOADER_CMS_RENDERING_INTENT=relative! \
SIXEL_LOADER_PREFER_8BIT=0 \
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/png_extended_trc builtin || {
    printf '\n%s\n' 'not ok 1 - builtin PNG preserves extended float32 ICC results'
    exit 0
}
printf '\n%s\n' 'ok 1 - builtin PNG preserves extended float32 ICC results'
exit 0

#!/bin/sh
# Policy: docs/loader/builtin-cms.md
# Verify premultiplied srgb with independent expected values.
set -eux
echo '1..1'
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" cms/0144_premultiplied_srgb || {
    printf '\n%s\n' 'not ok 1 - premultiplied_srgb'
    exit 0
}
printf '\n%s\n' 'ok 1 - premultiplied_srgb'
exit 0

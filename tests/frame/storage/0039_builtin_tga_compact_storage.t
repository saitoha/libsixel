#!/bin/sh
# Builtin TGA frames retain the full allocation after RGBA compaction.
# Policy: docs/concepts/pixelformat-precision/README.md
set -eux
echo '1..1'
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" frame/0014_clone_float_canonical tga-storage || {
    echo 'not ok 1 - Builtin TGA frames retain the full allocation after RGBA compaction'
    exit 0
}
echo 'ok 1 - Builtin TGA frames retain the full allocation after RGBA compaction'
exit 0

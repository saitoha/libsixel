#!/bin/sh
# Exercise the path adapters in the target runtime used by the test suite.
# Policy: docs/misc/platforms/windows.md
# Policy: docs/misc/platforms/windows-paths.md
# Policy: docs/misc/platforms/cygwin-msys.md
# Policy: docs/misc/platforms/emscripten.md
# Policy: docs/misc/platforms/cosmopolitan.md
# Coverage: WIN-02 WPATH-04 CYG-02 EM-02 COSMO-01

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "platform/path/0001_path_to_libc_runtime" || {
    echo "not ok 1 - target-runtime path adapters preserve their contract"
    exit 0
}

echo "ok 1 - target-runtime path adapters preserve their contract"
exit 0

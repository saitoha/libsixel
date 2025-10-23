#!/usr/bin/env bash
# Ensure informational commands succeed.
set -euo pipefail
SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
# shellcheck source=tests/converters/common.bash
source "${SCRIPT_DIR}/common.bash"

echo '[test3] print information'

# Confirm help output is accessible.
run_img2sixel -H
# Confirm version output is accessible.
run_img2sixel -V

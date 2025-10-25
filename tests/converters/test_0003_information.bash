#!/usr/bin/env bash
# Ensure informational commands succeed.
set -euo pipefail
SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
# shellcheck source=tests/converters/common.bash
source "${SCRIPT_DIR}/common.bash"

# ----------------------------------------------------------------------
#  +----------------------+----------------------------+
#  | Case                 | Expectation                 |
#  +----------------------+----------------------------+
#  | Help banner          | img2sixel -H runs cleanly   |
#  | Version banner       | img2sixel -V runs cleanly   |
#  +----------------------+----------------------------+
# ----------------------------------------------------------------------

tap_init "$(basename "$0")"
tap_plan 2

img2sixel_help_displays() {
    tap_log "[information] verifying -H output"
    run_img2sixel -H
}

img2sixel_version_displays() {
    tap_log "[information] verifying -V output"
    run_img2sixel -V
}

tap_case 'img2sixel -H prints help' img2sixel_help_displays
tap_case 'img2sixel -V prints version' img2sixel_version_displays

#!/usr/bin/env bash
# Validate img2sixel DCS parsing edge cases.
set -euo pipefail
SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
# shellcheck source=tests/converters/common.bash
source "${SCRIPT_DIR}/common.bash"

# ----------------------------------------------------------------------
#  +----------------------+------------------------------+
#  | Case                 | Expectation                   |
#  +----------------------+------------------------------+
#  | Tab-separated colors | Parsing tolerates \t introducers |
#  | Oversized geometry   | Parsing tolerates large sizes |
#  +----------------------+------------------------------+
# ----------------------------------------------------------------------

tap_init "$(basename "$0")"

target_image="${IMAGES_DIR}/snake.png"
require_file "${target_image}"

tap_plan 2

dcs_tab_separated_colours() {
    tap_log "[dcs-variations] tab separated colour introducers"
    run_img2sixel "${target_image}" | \
        sed 's/C/C:/g' | tr ':' '\t' | \
        run_img2sixel >/dev/null
}

dcs_oversized_geometry() {
    tap_log "[dcs-variations] oversized geometry values"
    run_img2sixel "${target_image}" | \
        sed 's/"1;1;600;450/"1;1;700;500/' | \
        run_img2sixel >/dev/null
}

tap_case 'DCS tolerates tab separators' dcs_tab_separated_colours
tap_case 'DCS tolerates oversized geometry' dcs_oversized_geometry

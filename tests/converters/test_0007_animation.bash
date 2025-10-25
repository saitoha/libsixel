#!/usr/bin/env bash
# Check animation related switches.
set -euo pipefail
SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
# shellcheck source=tests/converters/common.bash
source "${SCRIPT_DIR}/common.bash"

# ----------------------------------------------------------------------
#  +----------------------+-------------------------------+
#  | Case                 | Expectation                    |
#  +----------------------+-------------------------------+
#  | Update-only render   | -u cooperates with -ldisable   |
#  | Static frame render  | -g cooperates with -ldisable   |
#  | Combined render      | -u and -g coexist              |
#  | Sequence splitting   | -S runs with Atkinson dithering|
#  +----------------------+-------------------------------+
# ----------------------------------------------------------------------

tap_init "$(basename "$0")"

animation_source="${IMAGES_DIR}/seq2gif.gif"
require_file "${animation_source}"

declare -a ANIMATION_DESCRIPTIONS=()
declare -a ANIMATION_SNIPPETS=()

register_animation_case() {
    local description

    description=$1
    shift
    ANIMATION_DESCRIPTIONS+=("${description}")
    ANIMATION_SNIPPETS+=("$*")
}

execute_animation_case() {
    local snippet

    snippet=$1
    tap_log "[animation] ${snippet}"
    eval "${snippet}"
}

register_animation_case 'Update-only animation render' \
    'run_img2sixel -ldisable -dnone -u -lauto "${animation_source}"'
register_animation_case 'Static frame animation render' \
    'run_img2sixel -ldisable -dnone -g "${animation_source}"'
register_animation_case 'Combined update and static render' \
    'run_img2sixel -ldisable -dnone -u -g "${animation_source}"'
register_animation_case 'Sequence splitting with Atkinson diffusion' \
    'run_img2sixel -S -datkinson "${animation_source}"'

case_total=${#ANIMATION_DESCRIPTIONS[@]}
tap_plan "${case_total}"

for index in "${!ANIMATION_DESCRIPTIONS[@]}"; do
    description=${ANIMATION_DESCRIPTIONS[${index}]}
    snippet=${ANIMATION_SNIPPETS[${index}]}
    tap_case "${description}" execute_animation_case "${snippet}"
done

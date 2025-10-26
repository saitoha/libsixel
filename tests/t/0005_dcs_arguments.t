#!/usr/bin/env bash
# Test various DCS argument combinations.
set -euo pipefail
SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
# shellcheck source=tests/t/common.bash
source "${SCRIPT_DIR}/common.bash"

# ----------------------------------------------------------------------
#  +---------------------------+
#  | Prefix | Suffix | Result |
#  +---------------------------+
#  |   i    |   j    |  OK    |
#  +---------------------------+
#  The table above mirrors the nested loops below; each coordinate
#  becomes its own TAP entry.
# ----------------------------------------------------------------------

tap_init "$(basename "$0")"

require_file "${IMAGES_DIR}/map8.png"

case_count=0
for i in $(seq 0 10); do
    for j in $(seq 0 2); do
        case_count=$((case_count + 1))
    done
done

dcs_argument_case() {
    local prefix
    local suffix

    prefix=$1
    suffix=$2
    tap_log "[dcs] prefix=${prefix} suffix=${suffix}"
    run_img2sixel "${IMAGES_DIR}/map8.png" | \
        sed "s/Pq/P${prefix};;${suffix}q/" | \
        run_img2sixel >/dev/null
}

tap_plan "${case_count}"

for i in $(seq 0 10); do
    for j in $(seq 0 2); do
        tap_case "DCS prefix ${i} suffix ${j}" dcs_argument_case "${i}" "${j}"
    done
done

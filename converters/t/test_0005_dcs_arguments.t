#!/usr/bin/env bash
# Test various DCS argument combinations.
set -euo pipefail
SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
# shellcheck source=converters/t/common.t
source "${SCRIPT_DIR}/common.t"

echo '[test5] DCS arguments handling'

require_file "${IMAGES_DIR}/map8.png"

for i in $(seq 0 10); do
    for j in $(seq 0 2); do
        # Confirm arbitrary DCS prefix arguments are tolerated.
        run_img2sixel "${IMAGES_DIR}/map8.png" | \
            sed "s/Pq/P${i};;${j}q/" | \
            run_img2sixel >/dev/null
    done
done

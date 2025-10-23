#!/usr/bin/env bash
# Cover a range of PNG variations.
set -euo pipefail
SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
# shellcheck source=tests/converters/common.bash
source "${SCRIPT_DIR}/common.bash"

echo
echo '[test9] various PNG'

require_file "${IMAGES_DIR}/pngsuite/basic/basn0g01.png"
require_file "${IMAGES_DIR}/pngsuite/background/bgai4a08.png"

pngsuite_basic=(
    basic/basn0g01.png
    basic/basn0g02.png
    basic/basn0g04.png
    basic/basn0g08.png
    basic/basn0g16.png
    basic/basn3p01.png
    basic/basn3p02.png
    basic/basn3p04.png
    basic/basn3p08.png
    basic/basn4a08.png
    basic/basn4a16.png
    basic/basn6a08.png
    basic/basn6a16.png
)

pngsuite_background=(
    background/bgai4a08.png
    background/bgai4a16.png
    background/bgan6a08.png
    background/bgan6a16.png
    background/bgbn4a08.png
    background/bggn4a16.png
    background/bgwn6a08.png
    background/bgyn6a16.png
)

for rel in "${pngsuite_basic[@]}"; do
    run_img2sixel "${IMAGES_DIR}/pngsuite/${rel}"
done

for rel in "${pngsuite_basic[@]}"; do
    run_img2sixel -w32 "${IMAGES_DIR}/pngsuite/${rel}"
done

for rel in "${pngsuite_basic[@]}"; do
    run_img2sixel -c16x16+8+8 "${IMAGES_DIR}/pngsuite/${rel}"
done

for rel in "${pngsuite_background[@]}"; do
    run_img2sixel "${IMAGES_DIR}/pngsuite/${rel}"
done

for rel in "${pngsuite_background[@]}"; do
    run_img2sixel -B'#fff' "${IMAGES_DIR}/pngsuite/${rel}"
done

for rel in "${pngsuite_background[@]}"; do
    run_img2sixel -w32 -B'#fff' "${IMAGES_DIR}/pngsuite/${rel}"
done

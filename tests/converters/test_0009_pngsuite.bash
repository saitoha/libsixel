#!/usr/bin/env bash
# Cover a range of PNG variations.
set -euo pipefail
SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
# shellcheck source=tests/converters/common.bash
source "${SCRIPT_DIR}/common.bash"

# ----------------------------------------------------------------------
#  +------------------------------+-------------------------------+
#  | Suite                        | Transform                      |
#  +------------------------------+-------------------------------+
#  | basic                         | default, width=32, crop 16x16 |
#  | background                    | default, white bg, white+32   |
#  +------------------------------+-------------------------------+
# ----------------------------------------------------------------------

tap_init "$(basename "$0")"

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

for rel in "${pngsuite_basic[@]}" "${pngsuite_background[@]}"; do
    require_file "${IMAGES_DIR}/pngsuite/${rel}"
done

basic_cases=$(( ${#pngsuite_basic[@]} * 3 ))
background_cases=$(( ${#pngsuite_background[@]} * 3 ))
total_cases=$((basic_cases + background_cases))

tap_plan "${total_cases}"

pngsuite_basic_case() {
    local mode
    local rel
    local path

    mode=$1
    rel=$2
    path="${IMAGES_DIR}/pngsuite/${rel}"
    tap_log "[pngsuite-basic] mode=${mode} path=${rel}"
    case "${mode}" in
        default)
            run_img2sixel "${path}"
            ;;
        width32)
            run_img2sixel -w32 "${path}"
            ;;
        crop)
            run_img2sixel -c16x16+8+8 "${path}"
            ;;
        *)
            printf 'unexpected mode: %s\n' "${mode}"
            return 1
            ;;
    esac
}

pngsuite_background_case() {
    local mode
    local rel
    local path

    mode=$1
    rel=$2
    path="${IMAGES_DIR}/pngsuite/${rel}"
    tap_log "[pngsuite-background] mode=${mode} path=${rel}"
    case "${mode}" in
        default)
            run_img2sixel "${path}"
            ;;
        white)
            run_img2sixel -B"#fff" "${path}"
            ;;
        white_w32)
            run_img2sixel -w32 -B"#fff" "${path}"
            ;;
        *)
            printf 'unexpected mode: %s\n' "${mode}"
            return 1
            ;;
    esac
}

for rel in "${pngsuite_basic[@]}"; do
    tap_case "basic default ${rel}" pngsuite_basic_case default "${rel}"
    tap_case "basic width32 ${rel}" pngsuite_basic_case width32 "${rel}"
    tap_case "basic crop ${rel}" pngsuite_basic_case crop "${rel}"
done

for rel in "${pngsuite_background[@]}"; do
    tap_case "background default ${rel}" pngsuite_background_case default "${rel}"
    tap_case "background white ${rel}" pngsuite_background_case white "${rel}"
    tap_case "background white_w32 ${rel}" pngsuite_background_case white_w32 "${rel}"
done

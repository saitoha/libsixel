#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
# shellcheck source=converters/t/common.t
source "${SCRIPT_DIR}/common.t"

quality_err="${TMP_DIR}/assessment-quality.err"
quality_out="${TMP_DIR}/assessment-quality.out"

require_file "${IMAGES_DIR}/snake.jpg"
require_file "${IMAGES_DIR}/map8.png"

rm -f "${quality_err}" "${quality_out}"

echo '[test1] maintain assessment spool between consecutive inputs'

if run_img2sixel -a quality "${IMAGES_DIR}/snake.jpg" \
        "${IMAGES_DIR}/map8.png" \
        >"${quality_out}" 2>"${quality_err}"; then
    cat "${quality_err}" >&2 || :
    rm -f "${quality_err}" "${quality_out}"
    exit 1
fi

if [[ -s "${quality_out}" ]]; then
    cat "${quality_err}" >&2 || :
    rm -f "${quality_err}" "${quality_out}"
    exit 1
fi

rm -f "${quality_err}" "${quality_out}"

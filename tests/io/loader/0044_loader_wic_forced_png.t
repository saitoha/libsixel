#!/bin/sh
# TAP test confirming --loaders wic! forces PNG decoding path.

set -eu

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

feature_defined_in_config "HAVE_WIC" || {
    skip_all "wic loader is unavailable"
}

input_png="${TOP_SRCDIR}/tests/data/inputs/formats/rgba.png"

set +e
loader_output=$(run_img2sixel -L wic! "${input_png}" 2>&1 >/dev/null)
rc=$?
set -e

printf '%s' "${loader_output}" >&2

test "${RUNTIME_ENV_IS_WINE-0}" -eq 1 && {
    skip_all "WIC is unavailable under wine"
}

echo "1..1"
set -v

test "${rc}" -eq 0 || {
    fail 1 "wic forced PNG decoding failed"
    exit 0
}

pass 1 "wic forced PNG decoding succeeds"
exit 0

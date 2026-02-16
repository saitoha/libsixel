#!/bin/sh
# TAP test confirming GIF input fails with --loaders wic!.

set -eu

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

feature_defined_in_config "HAVE_WIC" || {
    skip_all "wic loader is unavailable"
}

input_png="${TOP_SRCDIR}/tests/data/inputs/formats/rgba.png"
input_gif="${TOP_SRCDIR}/tests/data/inputs/small.gif"

set +e
probe_output=$(run_img2sixel -L wic! "${input_png}" 2>&1 >/dev/null)
probe_rc=$?
set -e

printf '%s' "${probe_output}" >&2

printf '%s' "${probe_output}" \
    | grep "{cacaf262-9370-4615-a13b-9f5539da4c0a} not registered" \
    >/dev/null && {
    skip_all "WIC is not available"
}

echo "1..1"
set -v

test "${probe_rc}" -eq 0 || {
    fail 1 "wic runtime probe failed"
    exit 0
}

set +e
run_img2sixel -L wic! "${input_gif}" >/dev/null 2>&1
rc=$?
set -e

test "${rc}" -ne 0 || {
    fail 1 "wic forced GIF decoding should fail"
    exit 0
}

pass 1 "wic forced GIF decoding fails without fallback"
exit 0

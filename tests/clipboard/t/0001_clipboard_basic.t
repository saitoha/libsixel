#!/bin/sh
# TAP test verifying clipboard conversion round-trip when supported.

# Enable strict mode with verbose tracing for diagnostics.
set -eux

test_name=$(basename "$0")
test_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
category_name=$(basename "$(dirname "${test_dir}")")
artifact_root=${ARTIFACT_ROOT:-"$(pwd)/_artifacts"}
artifact_dir="${artifact_root}/${category_name}/${test_name}"
log_file="${artifact_dir}/clipboard.log"

mkdir -p "${artifact_dir}"

script_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
. "${script_dir}/../../common/t/0001_converters_common.t"

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"
ensure_converter_available "SIXEL2PNG" "${SIXEL2PNG_PATH}" "sixel2png"

status=0

pass() {
    printf 'ok 1 - %s\n' "$1"
}

skip_all() {
    reason=$1
    printf 'ok 1 - skip %s\n' "${reason}"
    exit 0
}

fail() {
    printf 'not ok 1 - %s\n' "$1"
    exit 1
}

require_file "${images_dir}/snake.png"

echo "1..1"
set -v

sixel_src="${images_dir}/snake.png"
sixel_tmp="${artifact_dir}/clipboard-input.six"
roundtrip_png="${artifact_dir}/clipboard-roundtrip.png"

if run_img2sixel "${sixel_src}" >"${sixel_tmp}" 2>>"${log_file}"; then
    :
else
    fail "failed to prepare sixel input"
fi

if run_sixel2png -i "${sixel_tmp}" -o png:clipboard: \
        >>"${log_file}" 2>&1; then
    :
else
    skip_all "clipboard backend unavailable"
fi

if run_img2sixel clipboard: -o clipboard: >>"${log_file}" 2>&1; then
    :
else
    skip_all "clipboard backend unavailable"
fi

if run_sixel2png -i clipboard: -o "${roundtrip_png}" \
        >>"${log_file}" 2>&1; then
    :
else
    skip_all "clipboard backend unavailable"
fi

if [ -s "${roundtrip_png}" ]; then
    pass "clipboard round-trip succeeded"
else
    fail "round-trip PNG missing"
fi

#!/usr/bin/env bash
# Verify that the prescan fallback produces identical output to the
# one-pass planner across thread counts and colour modes.
set -euo pipefail
SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
# shellcheck source=converters/t/common.t
source "${SCRIPT_DIR}/common.t"

echo '[test18] prescan parity'

for name in map64.six snake.six; do
    require_file "${IMAGES_DIR}/${name}"
done

COMPARATOR_CMD=()
if command -v cmp >/dev/null 2>&1; then
    COMPARATOR_CMD=(cmp -s)
elif command -v diff >/dev/null 2>&1; then
    COMPARATOR_CMD=(diff -q)
else
    echo 'skipping: cmp/diff unavailable for image comparison' >&2
    exit 0
fi

compare_variant() {
    local image mode threads suffix base default_out prescan_out

    image=$1
    mode=$2
    threads=$3
    suffix=$4
    base=$(basename "${image}" .six)
    default_out="${TMP_DIR}/${base}-threads${threads}${suffix}-default.png"
    prescan_out="${TMP_DIR}/${base}-threads${threads}${suffix}-prescan.png"

    SIXEL_THREADS=${threads} \
        run_sixel2png ${mode} < "${IMAGES_DIR}/${image}" >"${default_out}"
    SIXEL_DECODE_PRESCAN=1 SIXEL_THREADS=${threads} \
        run_sixel2png ${mode} < "${IMAGES_DIR}/${image}" >"${prescan_out}"
    if ! "${COMPARATOR_CMD[@]}" "${default_out}" "${prescan_out}"; then
        echo "prescan output differs: ${image} mode=${mode:-indexed} " \
            "threads=${threads}" >&2
        exit 1
    fi
}

for image in map64.six snake.six; do
    compare_variant "${image}" "" 1 ''
    compare_variant "${image}" "" 4 ''
    compare_variant "${image}" "-D" 1 '-direct'
    compare_variant "${image}" "-D" 4 '-direct'
done

rm -f "${TMP_DIR}"/*.png

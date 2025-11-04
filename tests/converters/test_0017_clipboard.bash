#!/usr/bin/env bash
# Validate clipboard round-trip when the desktop session exposes a clipboard
# backend. The script tolerates missing clipboard support by skipping the
# test instead of failing so automated environments without GUI access stay
# green.
set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
. "${SCRIPT_DIR}/common.bash"

SIXEL_SRC="${IMAGES_DIR}/autumn.png"
SIXEL_TMP="${TMP_DIR}/clipboard-input.six"
ROUNDTRIP_PNG="${TMP_DIR}/clipboard-roundtrip.png"

run_img2sixel "${SIXEL_SRC}" >"${SIXEL_TMP}" || exit 1

if ! run_sixel2png -i "${SIXEL_TMP}" -o png:clipboard:; then
    echo "clipboard backend not available; skipping" >&2
    exit 0
fi

if ! run_img2sixel clipboard: -o clipboard:; then
    echo "clipboard backend not available; skipping" >&2
    exit 0
fi

if ! run_sixel2png -i clipboard: -o "${ROUNDTRIP_PNG}"; then
    echo "clipboard backend not available; skipping" >&2
    exit 0
fi

if [[ ! -s "${ROUNDTRIP_PNG}" ]]; then
    echo "round-trip PNG is missing" >&2
    exit 1
fi

exit 0

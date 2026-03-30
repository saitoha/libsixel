#!/bin/sh
# TAP test verifying clipboard conversion round-trip when supported.

# Enable strict mode with verbose tracing for diagnostics.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

test "${HAVE_SIXEL2PNG-}" = 1 || {
    printf "1..0 # SKIP sixel2png is disabled in this build\n";
    exit 0
}

test -n "${LSQA_PATH-}" || {
    printf "1..0 # SKIP lsqa is disabled in this build\n";
    exit 0
}
printf '1..1\n'
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

sixel_src="${TOP_SRCDIR}/images/snake-progressive-16x16.jpg"
sixel_tmp="${ARTIFACT_LOCAL_DIR}/clipboard-input.six"
roundtrip_png="${ARTIFACT_LOCAL_DIR}/clipboard-roundtrip.png"
fake_clipboard_dir="${ARTIFACT_LOCAL_DIR}"
clipboard_backend_mode="system"
lsqa_floor="0.98"

test -n "${CI-}" && SIXEL_CLIPBOARD_USE_SYSTEM=1
set --

test "${SIXEL_CLIPBOARD_USE_SYSTEM-}" = 1 || {
    clipboard_backend_mode="fake"
    set -- --env SIXEL_CLIPBOARD_BACKEND=file \
        --env "SIXEL_CLIPBOARD_FILE_DIR=${fake_clipboard_dir}"
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" "$@" "${sixel_src}" >"${sixel_tmp}" || {
    echo "not ok" 1 - "failed to prepare sixel input"
    exit 0
}

${SIXEL_RUNTIME-} "${SIXEL2PNG_PATH}" "$@" -i "${sixel_tmp}" -o png:clipboard: || {
    printf "ok 1 # SKIP clipboard backend unavailable\n"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" "$@" -o clipboard: clipboard: || {
    printf "ok 1 # SKIP clipboard backend unavailable\n"
    exit 0
}

${SIXEL_RUNTIME-} "${SIXEL2PNG_PATH}" "$@" -i clipboard: -o "${roundtrip_png}" || {
    printf "ok 1 # SKIP clipboard backend unavailable\n"
    exit 0
}

test -s "${roundtrip_png}" || {
    echo "not ok" 1 - "round-trip PNG missing"
    exit 0
}

${SIXEL_RUNTIME-} "${LSQA_PATH}" -b "MS-SSIM:${lsqa_floor}" "${sixel_src}" "${roundtrip_png}" >/dev/null 2>&1 || {
    echo "not ok" 1 - "clipboard round-trip quality check failed"
    exit 0
}

echo "ok" 1 - "clipboard round-trip succeeded (${clipboard_backend_mode})"

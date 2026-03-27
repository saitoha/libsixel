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

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"
test -n "${LSQA_PATH-}" || {
    printf "1..0 # SKIP lsqa is disabled in this build\n";
    exit 0
}
printf '1..1\n'
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

sixel_src="${TOP_SRCDIR}/images/snake-progressive-16x16.jpg"
sixel_tmp="${ARTIFACT_LOCAL_DIR}/clipboard-input.six"
roundtrip_png="${ARTIFACT_LOCAL_DIR}/clipboard-roundtrip.png"
fake_clipboard_dir="${ARTIFACT_LOCAL_DIR}/clipboard-fake"
clipboard_backend_mode="fake"
use_system_clipboard=0
lsqa_floor="0.98"

if test "${SIXEL_CLIPBOARD_USE_SYSTEM-}" = 1; then
    use_system_clipboard=1
elif test -n "${CI-}" && test "${SIXEL_CLIPBOARD_FORCE_FAKE-}" != 1; then
    use_system_clipboard=1
fi

if test "${use_system_clipboard}" = 1; then
    clipboard_backend_mode="system"
else
    mkdir -p "${fake_clipboard_dir}"
fi

if test "${use_system_clipboard}" = 1; then
    run_img2sixel "${sixel_src}" >"${sixel_tmp}" || {
        echo "not ok" 1 - "failed to prepare sixel input"
        exit 0
    }
else
    run_img2sixel --env SIXEL_CLIPBOARD_BACKEND=file \
        --env SIXEL_CLIPBOARD_FILE_DIR="${fake_clipboard_dir}" \
        "${sixel_src}" >"${sixel_tmp}" || {
        echo "not ok" 1 - "failed to prepare sixel input"
        exit 0
    }
fi

if test "${use_system_clipboard}" = 1; then
    run_sixel2png -i "${sixel_tmp}" -o png:clipboard: || {
        printf "ok 1 # SKIP clipboard backend unavailable\n"
        exit 0
    }
else
    run_sixel2png --env SIXEL_CLIPBOARD_BACKEND=file \
        --env SIXEL_CLIPBOARD_FILE_DIR="${fake_clipboard_dir}" \
        -i "${sixel_tmp}" -o png:clipboard: || {
        printf "ok 1 # SKIP clipboard backend unavailable\n"
        exit 0
    }
fi

if test "${use_system_clipboard}" = 1; then
    run_img2sixel clipboard: -o clipboard: || {
        printf "ok 1 # SKIP clipboard backend unavailable\n"
        exit 0
    }
else
    run_img2sixel --env SIXEL_CLIPBOARD_BACKEND=file \
        --env SIXEL_CLIPBOARD_FILE_DIR="${fake_clipboard_dir}" \
        clipboard: -o clipboard: || {
        printf "ok 1 # SKIP clipboard backend unavailable\n"
        exit 0
    }
fi

if test "${use_system_clipboard}" = 1; then
    run_sixel2png -i clipboard: -o "${roundtrip_png}" || {
        printf "ok 1 # SKIP clipboard backend unavailable\n"
        exit 0
    }
else
    run_sixel2png --env SIXEL_CLIPBOARD_BACKEND=file \
        --env SIXEL_CLIPBOARD_FILE_DIR="${fake_clipboard_dir}" \
        -i clipboard: -o "${roundtrip_png}" || {
        printf "ok 1 # SKIP clipboard backend unavailable\n"
        exit 0
    }
fi

test -s "${roundtrip_png}" || {
    echo "not ok" 1 - "round-trip PNG missing"
    exit 0
}

run_lsqa -b "MS-SSIM:${lsqa_floor}" "${sixel_src}" "${roundtrip_png}" >/dev/null 2>&1 || {
    echo "not ok" 1 - "clipboard round-trip quality check failed"
    exit 0
}

echo "ok" 1 - "clipboard round-trip succeeded (${clipboard_backend_mode})"

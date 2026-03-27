#!/bin/sh
# TAP test verifying the file-backed clipboard backend used by local tests.

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
sixel_tmp="${ARTIFACT_LOCAL_DIR}/clipboard-file-input.six"
roundtrip_png="${ARTIFACT_LOCAL_DIR}/clipboard-file-roundtrip.png"
fake_clipboard_dir="${ARTIFACT_LOCAL_DIR}"
fake_png_slot="${fake_clipboard_dir}/image.bin"
fake_text_slot="${fake_clipboard_dir}/text.bin"
png_magic=
text_magic=
lsqa_floor="0.98"

set -- --env SIXEL_CLIPBOARD_BACKEND=file \
    --env "SIXEL_CLIPBOARD_FILE_DIR=${fake_clipboard_dir}"

run_img2sixel "$@" "${sixel_src}" >"${sixel_tmp}" || {
    echo "not ok" 1 - "failed to prepare sixel input"
    exit 0
}

run_sixel2png "$@" -i "${sixel_tmp}" -o png:clipboard: || {
    echo "not ok" 1 - "failed to write PNG into fake clipboard"
    exit 0
}

test -s "${fake_png_slot}" || {
    echo "not ok" 1 - "fake clipboard PNG slot missing"
    exit 0
}

png_magic=$(od -An -t x1 -N 8 "${fake_png_slot}" \
    | LC_ALL=C tr -d '[:space:]' \
    | LC_ALL=C tr 'A-F' 'a-f')
test "${png_magic}" = "89504e470d0a1a0a" || {
    echo "not ok" 1 - "fake clipboard PNG slot has invalid signature"
    exit 0
}

run_img2sixel "$@" clipboard: -o clipboard: || {
    echo "not ok" 1 - "failed to read/write fake clipboard SIXEL payload"
    exit 0
}

test -s "${fake_text_slot}" || {
    echo "not ok" 1 - "fake clipboard text slot missing"
    exit 0
}

text_magic=$(od -An -t x1 -N 2 "${fake_text_slot}" \
    | LC_ALL=C tr -d '[:space:]' \
    | LC_ALL=C tr 'A-F' 'a-f')
test "${text_magic}" = "1b50" || {
    echo "not ok" 1 - "fake clipboard text slot is not SIXEL text"
    exit 0
}

run_sixel2png "$@" -i clipboard: -o "${roundtrip_png}" || {
    echo "not ok" 1 - "failed to decode fake clipboard payload"
    exit 0
}

test -s "${roundtrip_png}" || {
    echo "not ok" 1 - "round-trip PNG missing"
    exit 0
}

run_lsqa -b "MS-SSIM:${lsqa_floor}" "${sixel_src}" "${roundtrip_png}" >/dev/null 2>&1 || {
    echo "not ok" 1 - "fake clipboard round-trip quality check failed"
    exit 0
}

echo "ok" 1 - "fake clipboard backend round-trip succeeded"

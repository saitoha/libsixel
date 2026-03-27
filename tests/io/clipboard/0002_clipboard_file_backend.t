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
printf '1..1\n'
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

sixel_src="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
sixel_tmp="${ARTIFACT_LOCAL_DIR}/clipboard-file-input.six"
roundtrip_png="${ARTIFACT_LOCAL_DIR}/clipboard-file-roundtrip.png"
fake_clipboard_dir="${ARTIFACT_LOCAL_DIR}/clipboard-fake"
fake_png_slot="${fake_clipboard_dir}/image.bin"
fake_text_slot="${fake_clipboard_dir}/text.bin"
png_magic=
text_magic=

mkdir -p "${fake_clipboard_dir}"

run_img2sixel_fake() {
    run_img2sixel --env SIXEL_CLIPBOARD_BACKEND=file \
        --env SIXEL_CLIPBOARD_FILE_DIR="${fake_clipboard_dir}" \
        "$@"
}

run_sixel2png_fake() {
    run_sixel2png --env SIXEL_CLIPBOARD_BACKEND=file \
        --env SIXEL_CLIPBOARD_FILE_DIR="${fake_clipboard_dir}" \
        "$@"
}

run_img2sixel_fake "${sixel_src}" >"${sixel_tmp}" || {
    echo "not ok" 1 - "failed to prepare sixel input"
    exit 0
}

run_sixel2png_fake -i "${sixel_tmp}" -o png:clipboard: || {
    echo "not ok" 1 - "failed to write PNG into fake clipboard"
    exit 0
}

test -s "${fake_png_slot}" || {
    echo "not ok" 1 - "fake clipboard PNG slot missing"
    exit 0
}

png_magic=$(od -An -tx1 -N8 "${fake_png_slot}" | tr -d ' \n')
test "${png_magic}" = "89504e470d0a1a0a" || {
    echo "not ok" 1 - "fake clipboard PNG slot has invalid signature"
    exit 0
}

run_img2sixel_fake clipboard: -o clipboard: || {
    echo "not ok" 1 - "failed to read/write fake clipboard SIXEL payload"
    exit 0
}

test -s "${fake_text_slot}" || {
    echo "not ok" 1 - "fake clipboard text slot missing"
    exit 0
}

text_magic=$(od -An -tx1 -N2 "${fake_text_slot}" | tr -d ' \n')
test "${text_magic}" = "1b50" || {
    echo "not ok" 1 - "fake clipboard text slot is not SIXEL text"
    exit 0
}

run_sixel2png_fake -i clipboard: -o "${roundtrip_png}" || {
    echo "not ok" 1 - "failed to decode fake clipboard payload"
    exit 0
}

test -s "${roundtrip_png}" || {
    echo "not ok" 1 - "round-trip PNG missing"
    exit 0
}

echo "ok" 1 - "fake clipboard backend round-trip succeeded"

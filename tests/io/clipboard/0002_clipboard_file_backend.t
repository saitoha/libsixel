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

test -n "${LSQA_PATH-}" || {
    printf "1..0 # SKIP lsqa is disabled in this build\n";
    exit 0
}
printf '1..1\n'
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

sixel_src="${TOP_SRCDIR}/images/snake-progressive-16x16.jpg"
sixel_tmp="${ARTIFACT_LOCAL_DIR}/clipboard-file-input.six"
roundtrip_png="${ARTIFACT_LOCAL_DIR}/clipboard-file-roundtrip.png"
fake_clipboard_dir="${ARTIFACT_LOCAL_DIR}"
fake_png_slot="${fake_clipboard_dir}/image.bin"
fake_text_slot="${fake_clipboard_dir}/text.bin"
png_signature_cksum=
text_prefix=
text_prefix_cksum=
lsqa_floor="0.98"

set -- --env SIXEL_CLIPBOARD_BACKEND=file \
    --env SIXEL_DEBUG_TEMP=1 \
    --env "SIXEL_CLIPBOARD_FILE_DIR=${fake_clipboard_dir}"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" "$@" "${sixel_src}" >"${sixel_tmp}" || {
    echo "not ok" 1 - "failed to prepare sixel input"
    exit 0
}

${SIXEL_RUNTIME-} "${SIXEL2PNG_PATH}" "$@" -i "${sixel_tmp}" -o png:clipboard: || {
    echo "not ok" 1 - "failed to write PNG into fake clipboard"
    exit 0
}

test -s "${fake_png_slot}" || {
    echo "not ok" 1 - "fake clipboard PNG slot missing"
    exit 0
}

png_signature_cksum=$(dd if="${fake_png_slot}" bs=1 count=8 2>/dev/null | cksum)
test "${png_signature_cksum}" = "4074750897 8" || {
    echo "not ok" 1 - "fake clipboard PNG slot has invalid signature"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" "$@" -o clipboard: clipboard: || {
    echo "not ok" 1 - "failed to read/write fake clipboard SIXEL payload"
    exit 0
}

test -s "${fake_text_slot}" || {
    echo "not ok" 1 - "fake clipboard text slot missing"
    exit 0
}

IFS=P read -r text_prefix _ < "${fake_text_slot}" || true
text_prefix_cksum=$(printf "%sP" "${text_prefix}" | cksum)
test "${text_prefix_cksum}" = "3058461199 2" || {
    echo "not ok" 1 - "fake clipboard text slot is not SIXEL text"
    exit 0
}

${SIXEL_RUNTIME-} "${SIXEL2PNG_PATH}" "$@" -i clipboard: -o "${roundtrip_png}" || {
    echo "not ok" 1 - "failed to decode fake clipboard payload"
    exit 0
}

test -s "${roundtrip_png}" || {
    echo "not ok" 1 - "round-trip PNG missing"
    exit 0
}

${SIXEL_RUNTIME-} "${LSQA_PATH}" -b "MS-SSIM:${lsqa_floor}" "${sixel_src}" "${roundtrip_png}" >/dev/null 2>&1 || {
    echo "not ok" 1 - "fake clipboard round-trip quality check failed"
    exit 0
}

echo "ok" 1 - "fake clipboard backend round-trip succeeded"

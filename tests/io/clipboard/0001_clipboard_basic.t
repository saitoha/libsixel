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

printf '1..1\n'
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

sixel_src="${TOP_SRCDIR}/images/snake-progressive-16x16.jpg"
sixel_tmp="${ARTIFACT_LOCAL_DIR}/clipboard-input.six"
roundtrip_png="${ARTIFACT_LOCAL_DIR}/clipboard-roundtrip.png"
# Each run needs empty clipboard slots: a stale text slot would shadow
# the newly written PNG when the backend selects an input representation.
fake_clipboard_dir="${ARTIFACT_LOCAL_DIR}/clipboard-$$"
mkdir -p "${fake_clipboard_dir}"
clipboard_backend_mode="system"
reference_png="${ARTIFACT_LOCAL_DIR}/reference.png"
reference_sixel="${ARTIFACT_LOCAL_DIR}/reference.six"

test -n "${CI-}" && SIXEL_CLIPBOARD_USE_SYSTEM=1
set --

test "${SIXEL_CLIPBOARD_USE_SYSTEM-}" = 1 || {
    clipboard_backend_mode="fake"
    set -- --env SIXEL_CLIPBOARD_BACKEND=file \
        --env "SIXEL_CLIPBOARD_FILE_DIR=${fake_clipboard_dir}"
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --lookup-policy=none "$@" "${sixel_src}" >"${sixel_tmp}" || {
    echo "not ok" 1 - "failed to prepare sixel input"
    exit 0
}

${SIXEL_RUNTIME-} "${SIXEL2PNG_PATH}" "$@" -i "${sixel_tmp}" -o png:clipboard: || {
    printf "ok 1 # SKIP clipboard backend unavailable\n"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --lookup-policy=none "$@" -o clipboard: clipboard: || {
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

# Full palette scanning removes unrelated lookup-cache approximation.
# Repeat the same codec stages through ordinary files. Only clipboard
# transport differs, so the decoded RGB samples must match exactly.
${SIXEL_RUNTIME-} "${SIXEL2PNG_PATH}" -i "${sixel_tmp}" \
    -o "${reference_png}" || {
    echo "not ok 1 - file reference PNG conversion failed"
    exit 0
}
${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --lookup-policy=none "${reference_png}" \
    >"${reference_sixel}" || {
    echo "not ok 1 - file reference SIXEL conversion failed"
    exit 0
}
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/image_values \
    "${reference_sixel}" "${roundtrip_png}" 'builtin:Enone!' 0 || {
    echo "not ok 1 - clipboard transport changed decoded RGB samples"
    exit 0
}

echo "ok" 1 - "clipboard round-trip succeeded (${clipboard_backend_mode})"

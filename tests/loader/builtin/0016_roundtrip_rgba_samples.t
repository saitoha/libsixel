#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Confirm PNG export preserves decoded SIXEL samples.
# Policy: docs/misc/platforms/solaris.md
# Coverage: SOL-04

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

# Exercise the RGBA source through both command-line converters.

image_path="${TOP_SRCDIR}/tests/data/inputs/formats/rgba.png"
output_sixel="${ARTIFACT_LOCAL_DIR}/rgba_roundtrip.six"
output_png="${ARTIFACT_LOCAL_DIR}/rgba_roundtrip.png"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=0 -Lbuiltin! "${image_path}" >"${output_sixel}" || {
    echo "not ok" 1 - "rgba roundtrip encode failed"
    exit 0
}

${SIXEL_RUNTIME-} "${SIXEL2PNG_PATH}" -i "${output_sixel}" -o "${output_png}" || {
    echo "not ok" 1 - "rgba roundtrip decode failed"
    exit 0
}

# PNG export must preserve the decoded SIXEL colors exactly. Source RGBA
# color/alpha interpretation is covered by the dedicated numeric cases.
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/image_values \
    "${output_sixel}" "${output_png}" 'builtin:Enone!' 0 || {
    echo "not ok 1 - PNG export changed the SIXEL samples"
    exit 0
}
echo "ok 1 - RGBA roundtrip preserves decoded SIXEL samples"
exit 0

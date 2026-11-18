#!/bin/sh
# Policy: docs/encoder/palette-construction/pipeline/README.md
# Verify the sampling solid suboption: on keeps default output, off drops
# solid-region augmentation, and long, short and environment spellings agree
# with command-line priority over the environment.
# Policy: docs/cli/design-policy.md

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..5"
set -v

test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

# snake_64 contains solid regions, so augmentation changes its palette.
input_image="${TOP_SRCDIR}/tests/data/inputs/snake_64.bmp"
prefix="${ARTIFACT_LOCAL_DIR}/0069-sampling-solid-$$"
common="--threading-policy=greedy:global_budget=1 -p 16 -Qheckbert -d none"

# shellcheck disable=SC2086
${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" ${common} \
    -o "${prefix}-default.six" "${input_image}"
# shellcheck disable=SC2086
${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" ${common} -4auto:solid=on \
    -o "${prefix}-on.six" "${input_image}"
# shellcheck disable=SC2086
${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" ${common} \
    --sampling-policy=adaptive-grid:solid=off \
    -o "${prefix}-off.six" "${input_image}"
# shellcheck disable=SC2086
${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" ${common} -4auto:Soff \
    -o "${prefix}-short.six" "${input_image}"
# shellcheck disable=SC2086
${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" ${common} \
    --env SIXEL_SAMPLING_SOLID=off \
    -o "${prefix}-env.six" "${input_image}"
# shellcheck disable=SC2086
${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" ${common} \
    --env SIXEL_SAMPLING_SOLID=off -4auto:solid=on \
    -o "${prefix}-env-cli.six" "${input_image}"

if cmp -s "${prefix}-default.six" "${prefix}-on.six"; then
    echo "ok 1 - solid=on keeps default output"
else
    echo "not ok 1 - solid=on changed default output"
fi
if cmp -s "${prefix}-default.six" "${prefix}-off.six"; then
    echo "not ok 2 - solid=off did not change the palette sample"
else
    echo "ok 2 - solid=off drops solid-region augmentation"
fi
if cmp -s "${prefix}-off.six" "${prefix}-short.six" &&
        cmp -s "${prefix}-off.six" "${prefix}-env.six"; then
    echo "ok 3 - long, short and environment spellings agree"
else
    echo "not ok 3 - solid spellings produce different output"
fi
if cmp -s "${prefix}-default.six" "${prefix}-env-cli.six"; then
    echo "ok 4 - command-line solid overrides environment"
else
    echo "not ok 4 - environment overrode command-line solid"
fi
# shellcheck disable=SC2086
if ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" ${common} -4auto:solid=maybe \
        -o "${prefix}-bad.six" "${input_image}" 2>/dev/null; then
    echo "not ok 5 - invalid solid value was accepted"
else
    echo "ok 5 - invalid solid value is rejected"
fi
rm -f "${prefix}"-*.six
exit 0

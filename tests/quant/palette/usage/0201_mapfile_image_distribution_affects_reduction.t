#!/bin/sh
# Verify image-mapfile pixel frequencies affect reduction above 256 colors.
# Policy: docs/functionality/external-palettes.md

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_16.png"
low_map="${ARTIFACT_LOCAL_DIR}/low-weight.ppm"
high_map="${ARTIFACT_LOCAL_DIR}/high-weight.ppm"
low_palette="${ARTIFACT_LOCAL_DIR}/low-weight.pal"
high_palette="${ARTIFACT_LOCAL_DIR}/high-weight.pal"

# Both maps contain the same 17x17 set of 289 colors.  Only the 1,000 extra
# pixels differ: one map weights RGB 60/60/0, the other RGB 180/180/0.
{
    printf 'P3\n1289 1\n255\n'
    i=0
    while test "${i}" -lt 17; do
        j=0
        while test "${j}" -lt 17; do
            printf '%s %s 0\n' "$((i * 15))" "$((j * 15))"
            j=$((j + 1))
        done
        i=$((i + 1))
    done
    i=0
    while test "${i}" -lt 1000; do
        printf '60 60 0\n'
        i=$((i + 1))
    done
} >"${low_map}"
{
    printf 'P3\n1289 1\n255\n'
    i=0
    while test "${i}" -lt 17; do
        j=0
        while test "${j}" -lt 17; do
            printf '%s %s 0\n' "$((i * 15))" "$((j * 15))"
            j=$((j + 1))
        done
        i=$((i + 1))
    done
    i=0
    while test "${i}" -lt 1000; do
        printf '180 180 0\n'
        i=$((i + 1))
    done
} >"${high_map}"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L builtin! --threads=1 \
    --gpu-policy=off -Wgamma -Ugamma -m "${low_map}" \
    -M pal-jasc:"${low_palette}" -o /dev/null "${input_image}" || {
    echo "not ok" 1 - "low-value weighted image mapfile failed"
    exit 0
}
${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L builtin! --threads=1 \
    --gpu-policy=off -Wgamma -Ugamma -m "${high_map}" \
    -M pal-jasc:"${high_palette}" -o /dev/null "${input_image}" || {
    echo "not ok" 1 - "high-value weighted image mapfile failed"
    exit 0
}

! cmp -s "${low_palette}" "${high_palette}" || {
    echo "not ok" 1 - "image mapfile reduction ignored pixel distribution"
    exit 0
}

echo "ok" 1 - "image mapfile pixel distribution affects color reduction"
exit 0

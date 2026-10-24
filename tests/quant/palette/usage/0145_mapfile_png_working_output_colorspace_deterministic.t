#!/bin/sh
# Verify PNG mapfile colorspace conversion remains deterministic.
# Policy: docs/encoder/palette-construction/external-palettes.md
# Policy: docs/encoder/output/output-colorspace.md
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

# CMS is disabled. These eight occupied RGB colors come directly from the
# PNG samples (the indexed fixture also has an unused white PLTE entry).
# With no binning/snap and fewer colors than the palette budget, native
# Heckbert keeps singleton colors through the linear working round trip.
# Export must recover source sRGB8, not byte-rounded linear coordinates.

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
mapfile_png="${TOP_SRCDIR}/images/map8-palette.png"
expected_palette='JASC-PAL
0100
8
0 0 0
3 171 185
3 175 3
123 102 248
149 157 3
159 3 3
190 3 180
191 191 191'

actual_palette=$(
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --cms-engine=none --threading-policy=balanced:global_budget=1 \
        --quantize-model=heckbert --binning-policy=none \
        --sampling-policy=full-frame --snap-policy=none --diffusion=none \
        --working-colorspace=linear --output-colorspace=linear \
        -m "${mapfile_png}" -M pal:- -o /dev/null "${input_image}"
) || {
    echo "not ok" 1 - "PNG mapfile export failed with -W/-U linear"
    exit 0
}
actual_palette=$(printf "%s" "${actual_palette}" | tr -d '\015') || {
    echo "not ok" 1 - "PNG mapfile output normalization failed"
    exit 0
}

test "${actual_palette}" = "${expected_palette}" || {
    echo "not ok" 1 - "PNG mapfile converted palette changed under -W/-U linear"
    exit 0
}

echo "ok" 1 - "PNG mapfile colorspace conversion is deterministic"
exit 0

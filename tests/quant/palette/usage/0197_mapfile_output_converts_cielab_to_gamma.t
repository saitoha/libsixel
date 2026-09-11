#!/bin/sh
# Verify -M converts a CIELAB working palette to gamma RGB for export.
# Policy: docs/functionality/external-palettes.md
# Policy: docs/functionality/working-colorspace.md
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_16.png"
expected_palette='GIMP Palette
Name: libsixel export
Columns: 16
# Exported by libsixel
106  92  53	Index 0
 64  50   0	Index 1
126 117  69	Index 2
101  88  13	Index 3
141 167 122	Index 4
148 138 120	Index 5
151 134  13	Index 6
140 130  93	Index 7'

actual_palette=$(
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        -L builtin! --threads=1 --precision=float32 \
        --sampling-policy=full-frame --binning-policy=hard \
        --quantize-model=kmeans:seed=1:binbits=6 --merge-policy=none \
        --cover-policy=off --snap-policy=none --lookup-policy=none \
        --gpu-policy=off --diffusion=none -p '8!' \
        -Xgamma -Wcielab -Ugamma -M gpl:- -o /dev/null "${input_image}"
) || {
    echo "not ok" 1 - "CIELAB working palette export failed"
    exit 0
}
actual_palette=$(printf "%s" "${actual_palette}" | tr -d '\015') || {
    echo "not ok" 1 - "CIELAB palette output normalization failed"
    exit 0
}

test "${actual_palette}" = "${expected_palette}" || {
    echo "not ok" 1 - "CIELAB working values leaked into GPL output"
    exit 0
}

echo "ok" 1 - "GPL output contains gamma RGB instead of CIELAB values"
exit 0

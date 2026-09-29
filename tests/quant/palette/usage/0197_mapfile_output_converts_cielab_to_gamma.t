#!/bin/sh
# Verify -M converts a CIELAB working palette to gamma RGB for export.
# Policy: docs/encoder/palette-construction/external-palettes.md
# Policy: docs/encoder/palette-application/working-colorspace/README.md
# Policy: docs/encoder/output/output-colorspace.md
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

# Black and white fix both entries independently of clustering. The selected
# float path retains neutral Lab coordinates; exporting must give exact
# sRGB8 endpoints, without reconstructing a/b from their biased byte codes.
expected_palette='GIMP Palette
Name: libsixel export
Columns: 16
# Exported by libsixel
255 255 255	Index 0
  0   0   0	Index 1'
# Generated palettes do not promise the solver's ordering of these entries.
reversed_palette='GIMP Palette
Name: libsixel export
Columns: 16
# Exported by libsixel
  0   0   0	Index 0
255 255 255	Index 1'

actual_palette=$(
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        -L builtin! --cms-engine=none --threads=1 --precision=float32 \
        --sampling-policy=full-frame --binning-policy=none \
        --quantize-model=kmeans:seed=1 --merge-policy=none \
        --cover-policy=off --snap-policy=none --lookup-policy=none \
        --gpu-policy=off --diffusion=none -p '2!' \
        -Xgamma -Wcielab -Ugamma -M gpl:- -o /dev/null - <<'PPM'
P3
2 1
255
0 0 0 255 255 255
PPM
) || {
    echo "not ok" 1 - "CIELAB working palette export failed"
    exit 0
}
actual_palette=$(printf "%s" "${actual_palette}" | tr -d '\015') || {
    echo "not ok" 1 - "CIELAB palette output normalization failed"
    exit 0
}

test "${actual_palette}" = "${expected_palette}" || \
test "${actual_palette}" = "${reversed_palette}" || {
    echo "not ok" 1 - "CIELAB working values leaked into GPL output"
    exit 0
}

echo "ok" 1 - "GPL output contains gamma RGB instead of CIELAB values"
exit 0

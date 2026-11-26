#!/bin/sh
# Policy: docs/encoder/palette-application/working-colorspace/README.md
# The PAL interchange file carries sRGB8 even when output targets Display P3.
set -eux
echo '1..1'
set -v

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf '\n%s\n' 'ok 1 # SKIP img2sixel unavailable'
    exit 0
}
actual=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --threading-policy=greedy:global_budget=1 --precision-policy=float32 \
    -7 -Lbuiltin! -Udisplay-p3 -_none -~none -Fnone -trgb \
    -Qheckbert:cut=median -fnorm -saverage -p2 \
    "-Xicc:${TOP_SRCDIR}/tests/data/color-numeric/display-p3.icc" \
    -Wlinear -aoff -dnone --binning-policy=none --sampling-policy=full-frame \
    -Mpal:- -o/dev/null "${TOP_SRCDIR}/tests/data/color-numeric/icc-heckbert-samples.ppm") || {
    printf '\n%s\n' 'not ok 1 - ICC palette export'
    exit 0
}
# Compare the PAL header and both RGB8 entries independently of solver order.
awk '{ sub(/\r$/, "") }
    NR == FNR { expected[FNR] = $0; next }
    FNR <= 3 && $0 != expected[FNR] { exit 1 }
    FNR > 3 { seen[$0]++ }
    END { if (FNR != 5 || seen[expected[4]] != 1 || seen[expected[5]] != 1) exit 1 }
' "${TOP_SRCDIR}/tests/data/color-numeric/icc-heckbert-srgb8.pal" - <<EOF || {
$actual
EOF
    printf '%s\n' "$actual" >&2
    printf '\n%s\n' 'not ok 1 - exported ICC representatives differ from sRGB8'
    exit 0
}
printf '\n%s\n' 'ok 1 - ICC representatives exported in sRGB8'
exit 0

#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify RGB8 multi-layer fallback renders GdFl non-pixel fill payload.
# Reference generation commands:
#   python3 tests/data/inputs/formats/generate_psd_snake16_fixtures.py
#   python3 - <<'PY'
#   import math
#   w=h=16
#   c0=(255,32,32); c1=(32,64,255)
#   def g2l(v):
#       s=v/255.0
#       return s/12.92 if s<=0.04045 else ((s+0.055)/1.055)**2.4
#   def l2g(v):
#       v=max(0.0,min(1.0,v))
#       s=12.92*v if v<=0.0031308 else 1.055*(v**(1.0/2.4))-0.055
#       return int(round(max(0.0,min(1.0,s))*255.0))
#   l0=[g2l(x) for x in c0]; l1=[g2l(x) for x in c1]
#   out=bytearray(f"P6\\n{w} {h}\\n255\\n".encode("ascii"))
#   for y in range(h):
#       for x in range(w):
#           t=(x+0.5)/w
#           r=l0[0]*(1.0-t)+l1[0]*t
#           g=l0[1]*(1.0-t)+l1[1]*t
#           b=l0[2]*(1.0-t)+l1[2]*t
#           out.extend((l2g(r), l2g(g), l2g(b)))
#   open("tests/data/loader/builtin_expected/"
#        "psd_snake16_multilayer_fill_gdfl_expected.ppm","wb").write(out)
#   PY

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

input_psd="${TOP_SRCDIR}/tests/data/inputs/formats/snake16_rgb8_missing_composite_multilayer_fill_gdfl.psd"
reference_ppm="${TOP_SRCDIR}/tests/data/loader/builtin_expected/psd_snake16_multilayer_fill_gdfl_expected.ppm"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L builtin! "${input_psd}" >/dev/null || {
    echo "not ok" 1 - "builtin loader failed to decode RGB8 fill(GdFl) PSD"
    exit 0
}

# This contract concerns decoded samples, before palette quantization.
# The independent 8-bit PPM and decoded 8-bit samples can each round by
# half a level; allow one RGB byte level plus float conversion roundoff.
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/image_values \
    "${reference_ppm}" "${input_psd}" 'builtin:cms_engine=auto!' 1.01 || {
    echo "not ok 1 - decoded PSD samples differ from the independent PPM"
    exit 0
}

echo "ok" 1 - "RGB8 fill(GdFl) decode matches reference pixel values"
exit 0

#!/bin/sh
# Verify lossy WebP ICC input keeps YUV/RGB decode paths close with cms=1.

set -eux

test "${HAVE_WEBP-}" = 1 || {
    printf "1..0 # SKIP libwebp support is disabled in this build\n"
    exit 0
}

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

# Real 192px input supports all five MS-SSIM scales. External dwebp/LCMS
# references and spatially shifted controls are generated offline; see
# tests/data/loader/quality/generate_webp_yuv_references.py.
input_webp="${TOP_SRCDIR}/tests/data/loader/quality/webp-yuv-icc.webp"
reference="${TOP_SRCDIR}/tests/data/loader/quality/webp-yuv-icc.ppm"
shifted="${TOP_SRCDIR}/tests/data/loader/quality/webp-yuv-icc-shifted.ppm"
output_yuv="${ARTIFACT_LOCAL_DIR}/webp-icc-yuv.sixel"
output_rgb="${ARTIFACT_LOCAL_DIR}/webp-icc-rgb.sixel"

# An unmanaged lossy source needs an explicit float request to select YUV.
message=$(set +xv; SIXEL_TRACE_TOPIC=webp_decode _SIXEL_TEST_LIBWEBP_FORCE_RGB_DECODE=0 \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --precision-policy=float32 --threading-policy=greedy:global_budget=1 --lookup-policy=none \
    -Llibwebp:cms_engine=auto! "${input_webp}" 2>&1 >"${output_yuv}") || {
    echo "not ok 1 - YUV decode failed: $message"
    exit 0
}
case "$message" in *"static decode path=lossy_yuv "*) true ;; *) false ;; esac || {
    echo "not ok 1 - expected lossy_yuv path: $message"
    exit 0
}
message=$(set +xv; SIXEL_TRACE_TOPIC=webp_decode _SIXEL_TEST_LIBWEBP_FORCE_RGB_DECODE=1 \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --precision-policy=float32 --threading-policy=greedy:global_budget=1 --lookup-policy=none \
    -Llibwebp:cms_engine=auto! "${input_webp}" 2>&1 >"${output_rgb}") || {
    echo "not ok 1 - RGB decode failed: $message"
    exit 0
}
case "$message" in *"static decode path=rgb_u8 "*) true ;; *) false ;; esac || {
    echo "not ok 1 - expected rgb_u8 path: $message"
    exit 0
}

# The reference-product metric uses the project quality gate. The historical
# small-image arithmetic-average thresholds did not measure this contract.
message=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" -m MS-SSIM -b MS-SSIM:0.98 "$output_yuv" "$reference" 2>&1) || {
    echo "not ok 1 - YUV quality: $message"
    exit 0
}
message=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" -m MS-SSIM -b MS-SSIM:0.98 "$output_rgb" "$reference" 2>&1) || {
    echo "not ok 1 - RGB quality: $message"
    exit 0
}
message=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" -m MS-SSIM -b MS-SSIM:0.98 "$output_yuv" "$output_rgb" 2>&1) || {
    echo "not ok 1 - YUV/RGB agreement: $message"
    exit 0
}
# Qualify the negative control as a valid finite measurement before requiring
# its rejection. An unavailable score must not count as defect detection.
message=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" -m MS-SSIM -b MS-SSIM:0 "$output_yuv" "$shifted" 2>&1) || {
    echo "not ok 1 - invalid shifted control: $message"
    exit 0
}
! ${SIXEL_RUNTIME-} "${LSQA_PATH}" -m MS-SSIM -b MS-SSIM:0.98 "$output_yuv" "$shifted" >/dev/null 2>&1 || {
    echo "not ok 1 - shifted control unexpectedly passed"
    exit 0
}
echo "ok 1 - distinct YUV/RGB paths meet independent quality gate"
exit 0

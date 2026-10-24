#!/bin/sh
# Early snap exercises ordinary float dispatch after a completed solver build.
# Policy: docs/encoder/palette-construction/quantization/README.md
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

status=0
trace=$(set +xv; SIXEL_TRACE_TOPIC=palette_contract \
    _SIXEL_TEST_PALETTE_QUANTIZER_POST_BUILD_FAILURE=kmeans-float32 \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --threading-policy=balanced:global_budget=1 \
    --binning-policy=hard -Qkmeans --precision-policy=float32 \
    -_nearest:Iall -dnone -p16 "-~none" -Lbuiltin -ldisable \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" 2>&1) || status=$?

test "$status" -ne 0 || {
    echo "not ok 1 - failed float K-means build was silently retried"
    exit 0
}

case "${trace}" in *LSXBPS1\|quantizer_requested=2\|quantizer_effective=-1\|quantizer_origin=explicit\|quantizer_phase=unresolved\|quantizer_reason=none\|binning_requested=hard\|binning_effective=unset\|binning_origin=explicit\|binning_phase=unresolved\|binning_reason=none\|points=0\|quantizer_retries=0*) true ;; *) false ;; esac || {
    echo "not ok 1 - failed float K-means build published success state"
    exit 0
}

echo "ok 1 - failed float K-means preserves the error and uncommitted state"
exit 0

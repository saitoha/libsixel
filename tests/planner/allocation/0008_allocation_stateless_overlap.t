#!/bin/sh
# Policy: docs/threading/encoder.md
# Stateless policies retain exact pixels with automatic or explicit overlap.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled"
    set -v
    exit 0
}
test "${SIXEL_ENABLE_THREADS-0}" = 1 || {
    echo "1..0 # SKIP threading is disabled"
    set -v
    exit 0
}
echo "1..1"
set -v

# Every output below is consumed by the decoded-pixel comparison.
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

export SIXEL_DITHER_PARALLEL_THREADS_MAX=
export SIXEL_DITHER_PARALLEL_BAND_WIDTH=
export SIXEL_DITHER_PARALLEL_BAND_OVERWRAP=

# Sixteen colors exercise the low-color geometry where overlap is otherwise
# useful for diffusion. This is exact pixel coverage, not a quality gate.
set -- 'none --precision=8bit' 'none -Xc' \
    'a_dither --precision=8bit' 'a_dither -Xc' \
    'x_dither --precision=8bit' 'x_dither -Xc' \
    'bluenoise --precision=8bit' 'bluenoise -Xc'
while test "$#" -gt 0; do
    kernel=${1%% *}
    precision=${1#* }
    shift
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Goff -Efast -=1 -p16 \
        "${precision}" -d "${kernel}" \
        "${TOP_SRCDIR}/tests/data/inputs/snake_64.png" \
        >"${ARTIFACT_LOCAL_DIR}/allocation-serial.six" || {
        echo "not ok 1 - serial ${kernel} conversion failed"
        exit 0
    }
    trace=$(set +xv; SIXEL_TRACE_TOPIC=dither_contract \
        ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Goff -Efast -=4 -p16 \
        "${precision}" -d "${kernel}" \
        "${TOP_SRCDIR}/tests/data/inputs/snake_64.png" \
        2>&1 >"${ARTIFACT_LOCAL_DIR}/allocation-default.six") || {
        echo "not ok 1 - automatic ${kernel} conversion failed"
        exit 0
    }
    case "${trace}" in
        *LSXDTH1\|*band_overlap=0\|*) true ;;
        *) false ;;
    esac || {
        echo "not ok 1 - ${kernel} automatic overlap is not zero"
        exit 0
    }
    trace=$(set +xv; SIXEL_TRACE_TOPIC=dither_contract \
        ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Goff -Efast -=4 -p16 \
        "${precision}" -d "${kernel}:band_overwrap=6" \
        "${TOP_SRCDIR}/tests/data/inputs/snake_64.png" \
        2>&1 >"${ARTIFACT_LOCAL_DIR}/allocation-explicit.six") || {
        echo "not ok 1 - explicit ${kernel} conversion failed"
        exit 0
    }
    case "${trace}" in
        *LSXDTH1\|*band_overlap=6\|*) true ;;
        *) false ;;
    esac || {
        echo "not ok 1 - ${kernel} explicit overlap was ignored"
        exit 0
    }
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
        planner/allocation/0008_allocation_stateless_overlap \
        "${ARTIFACT_LOCAL_DIR}/allocation-serial.six" \
        "${ARTIFACT_LOCAL_DIR}/allocation-default.six" \
        "${ARTIFACT_LOCAL_DIR}/allocation-explicit.six" || {
        echo "not ok 1 - ${kernel} decoded pixels differ"
        exit 0
    }
done

echo "ok 1 - stateless policies preserve pixels across overlap choices"
exit 0

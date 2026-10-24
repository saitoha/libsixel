#!/bin/sh
# Lazy still output must retain serial dither history while encoding overlaps.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled"
    exit 0
}
test "${SIXEL_ENABLE_THREADS-0}" = 1 || {
    echo "1..0 # SKIP threading is disabled"
    exit 0
}
echo "1..1"
set -v

test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

serial_output="${ARTIFACT_LOCAL_DIR}/0023-lazy-still-serial-$$.six"
lazy_output="${ARTIFACT_LOCAL_DIR}/0023-lazy-still-lazy-$$.six"
${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -d fs  \
    -=balanced:B1 -o "${serial_output}" \
    "${TOP_SRCDIR}/images/snake.png" || {
    echo "not ok 1 - serial reference failed"
    exit 0
}
trace=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -v -d fs  \
    -=lazy:B8 -o "${lazy_output}" \
    "${TOP_SRCDIR}/images/snake.png" 2>&1 >/dev/null) || {
    echo "not ok 1 - lazy conversion failed"
    exit 0
}
case "${trace}" in *threads:\ dither=1\ encode=7*) true ;; *) false ;; esac || {
    echo "not ok 1 - lazy did not retain parallel encoding with one producer"
    exit 0
}
cmp -s "${serial_output}" "${lazy_output}" || {
    echo "not ok 1 - lazy still differs from one-thread output"
    exit 0
}
test -s "${lazy_output}" || {
    echo "not ok 1 - lazy output is empty"
    exit 0
}
echo "ok 1 - lazy still overlaps encoding with serial-equivalent output"
exit 0

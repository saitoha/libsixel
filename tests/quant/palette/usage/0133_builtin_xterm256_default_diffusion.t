#!/bin/sh
# Verify the default diffusion is applied to the fixed palette.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled"
    exit 0
}
echo "1..1"
set -v

test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"
input_image="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
default_output="${ARTIFACT_LOCAL_DIR}/default.six"
explicit_output="${ARTIFACT_LOCAL_DIR}/explicit.six"
undiffused_output="${ARTIFACT_LOCAL_DIR}/undiffused.six"

# Auto diffusion selects fs for this fixed palette's color count.
trace=$(SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} \
    "${IMG2SIXEL_PATH}" -bxterm256 "${input_image}" \
    2>&1 >"${default_output}") || {
    echo "not ok 1 - default diffusion conversion failed"
    exit 0
}
case "${trace}" in
    *LSXDTH1\|rc=0\|diffuse=fs\|*\|apply=1\|consume=1\|*) ;;
    *)
        echo "not ok 1 - default diffusion did not reach the pixel consumer"
        exit 0
        ;;
esac
${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -bxterm256 \
    -d fs "${input_image}" >"${explicit_output}" || {
    echo "not ok 1 - explicit diffusion conversion failed"
    exit 0
}
cmp -s "${default_output}" "${explicit_output}" || {
    echo "not ok 1 - default and explicit diffusion outputs differ"
    exit 0
}
${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -bxterm256 \
    -d none "${input_image}" >"${undiffused_output}" || {
    echo "not ok 1 - undiffused control conversion failed"
    exit 0
}
cmp -s "${default_output}" "${undiffused_output}" && {
    echo "not ok 1 - default diffusion did not affect pixel output"
    exit 0
}
echo "ok 1 - fixed palette applies the default diffusion"
exit 0

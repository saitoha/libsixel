#!/bin/sh
# Verify reqcolors=256 keeps pal8 work format in builtin path.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}


echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_png="${TOP_SRCDIR}/tests/data/inputs/formats/libpng-pal8-trns-multi0-semi-icc.png"
out_hi="${ARTIFACT_LOCAL_DIR}/builtin-req256-cms0.six"
log_hi="${ARTIFACT_LOCAL_DIR}/builtin-req256-cms0.log"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -v -Lbuiltin:cms_engine=none! \
              -B#ffffff -d none -p256 \
              "${input_png}" >"${out_hi}" 2>"${log_hi}" || {
    echo "not ok 1 - builtin reqcolors=256 render failed"
    exit 0
}

pal8_path_found=0
while IFS= read -r line; do
    case "${line}" in
        *"formats: source=pal8 work=pal8"*)
            pal8_path_found=1
            break
            ;;
    esac
done < "${log_hi}"

test "${pal8_path_found}" -eq 1 || {
    echo "not ok 1 - builtin reqcolors=256 did not keep pal8 work format"
    exit 0
}

echo "ok 1 - builtin reqcolors=256 keeps pal8 work format"

exit 0

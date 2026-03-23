#!/bin/sh
# TAP test: libtiff CMS output tracks CoreGraphics TIFF RGB matrix references.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

test "${HAVE_LIBTIFF-}" = 1 || {
    printf "1..0 # SKIP libtiff loader is unavailable\n"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

input_dir="${TOP_SRCDIR}/tests/data/colormgmt/input/tiff/rgb"
reference_dir="${TOP_SRCDIR}/tests/data/colormgmt/reference/tiff/rgb"
mkdir -p "${ARTIFACT_LOCAL_DIR}"

set -- "${input_dir}"/*.tiff
if [ "$1" = "${input_dir}/*.tiff" ]; then
    printf "1..0 # SKIP TIFF RGB colormgmt matrix fixture is missing\n"
    exit 0
fi

echo "1..$#"
set -v

index=1
for input_tiff in "$@"; do
    name=$(basename "${input_tiff}" .tiff)
    reference_six="${reference_dir}/${name}.six"
    output_six="${ARTIFACT_LOCAL_DIR}/${name}_libtiff.six"
    lsqa_floor=0.999

    case "${name}" in
        *_icc1_*)
            lsqa_floor=0.996
            ;;
    esac

    if [ ! -f "${reference_six}" ]; then
        echo "not ok" "${index}" - "missing reference fixture: ${name}"
        index=$((index + 1))
        continue
    fi

    run_img2sixel -Llibtiff:cms=1! "${input_tiff}" >"${output_six}" || {
        echo "not ok" "${index}" - "libtiff decode failed: ${name}"
        index=$((index + 1))
        continue
    }

    lsqa_msg=$(set +xv; run_lsqa -m MS-SSIM -b "MS-SSIM:${lsqa_floor}" \
        "${reference_six}" "${output_six}" 2>&1) || {
        echo "not ok" "${index}" - "${name}: ${lsqa_msg}"
        index=$((index + 1))
        continue
    }

    echo "ok" "${index}" - "libtiff RGB colormgmt matrix matches reference: ${name}"
    index=$((index + 1))
done

exit 0

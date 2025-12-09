#!/bin/sh
# TAP test covering PNGSuite samples under various conversion modes.

# Enable strict mode with verbose tracing for diagnostics.
set -euxv

test_name=$(basename "$0")
artifact_root=${ARTIFACT_ROOT:-"$(pwd)/_artifacts"}
artifact_dir="${artifact_root}/${test_name}"
log_file="${artifact_dir}/pngsuite.log"
output_dir="${artifact_dir}/outputs"

tmp_dir="${artifact_dir}/tmp"

mkdir -p "${output_dir}" "${tmp_dir}"

script_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
. "${script_dir}/converters-common.t"

status=0
case_id=1

# Ensure the converter binary is present and libpng support is enabled;
# PNGSuite coverage requires the PNG loader to be built for this config.
ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"
ensure_feature_available "HAVE_LIBPNG" "png" "libpng support"

pass() {
    printf 'ok %s - %s\n' "$1" "$2"
}

fail() {
    printf 'not ok %s - %s\n' "$1" "$2"
    status=1
}

echo "1..6"

pngsuite_basic="basic/basn0g01.png basic/basn0g02.png basic/basn0g04.png \
    basic/basn0g08.png basic/basn0g16.png basic/basn3p01.png \
    basic/basn3p02.png basic/basn3p04.png basic/basn3p08.png \
    basic/basn4a08.png basic/basn4a16.png basic/basn6a08.png \
    basic/basn6a16.png"

pngsuite_background="background/bgai4a08.png background/bgai4a16.png \
    background/bgan6a08.png background/bgan6a16.png background/bgbn4a08.png \
    background/bggn4a16.png background/bgwn6a08.png background/bgyn6a16.png"

ensure_pngsuite_samples_present() {
    required_files=${pngsuite_basic}
    required_files="${required_files} ${pngsuite_background}"

    missing_rel=""

    for rel in ${required_files}; do
        if [ ! -f "${images_dir}/pngsuite/${rel}" ]; then
            missing_rel=${rel}
            break
        fi
    done

    if [ -n "${missing_rel}" ]; then
        skip_all "pngsuite sample '${missing_rel}' is missing"
    fi
}

ensure_pngsuite_samples_present

run_group() {
    files=$1
    description=$2
    mode=$3
    all_ok=1

    for rel in ${files}; do
        base=$(basename "${rel}")
        case_dir=${output_dir}/case${case_id}
        mkdir -p "${case_dir}"

        if run_img2sixel ${mode} "${images_dir}/pngsuite/${rel}" \
                >"${case_dir}/${base}.sixel" 2>>"${log_file}"; then
            :
        else
            all_ok=0
        fi
    done

    if [ ${all_ok} -eq 1 ]; then
        pass ${case_id} "${description}"
    else
        fail ${case_id} "${description}"
    fi
    case_id=$((case_id + 1))
}

run_group "${pngsuite_basic}" "basic samples convert" ""
run_group "${pngsuite_basic}" "basic samples with width 32" "-w32"
run_group "${pngsuite_basic}" "basic samples cropped" "-c16x16+8+8"
run_group "${pngsuite_background}" "background samples convert" ""
run_group "${pngsuite_background}" "background samples white" "-B#fff"
run_group "${pngsuite_background}" "background width 32 white" "-w32 -B#fff"

exit "${status}"

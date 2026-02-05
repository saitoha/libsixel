#!/bin/sh
# Shared helpers for PNGSuite loader tests.

set -eu

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

ensure_pngsuite_prereqs() {
    ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"
    ensure_feature_available "HAVE_LIBPNG" "png" "libpng support"
    ensure_pngsuite_samples_present
}

convert_pngsuite_group() {
    files=$1
    description=$2
    mode=$3
    output_dir=$4
    log_file=$5

    for rel in ${files}; do
        base=${rel##*/}
        if run_img2sixel ${mode} "${images_dir}/pngsuite/${rel}" \
                >"${output_dir}/${base}.sixel"; then
            :
        else
            return 1
        fi
    done

    return 0
}

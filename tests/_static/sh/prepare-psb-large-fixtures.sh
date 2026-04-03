#!/bin/sh

set -eu

if [ "$#" -lt 1 ] || [ "$#" -gt 2 ]; then
    printf '%s\n' "usage: $0 TOP_SRCDIR [TOP_BUILDDIR]" >&2
    exit 2
fi

top_srcdir=$1
top_builddir=${2:-$1}
src_formats_dir="${top_srcdir}/tests/data/inputs/formats"
dst_formats_dir="${top_builddir}/tests/data/inputs/formats"
webp_padded="${dst_formats_dir}/webp-static-icc-overlimit-padded.webp"
webp_padded_gz="${src_formats_dir}/webp-static-icc-overlimit-padded.webp.gz"

mkdir -p "${dst_formats_dir}"

fixtures_ready=1
for mode_prefix in cmyk mode7_cmyk; do
    for depth_tag in 16 32; do
        base="snake16_psb_${mode_prefix}${depth_tag}_missing_composite_multilayer"
        for suffix in \
            "normal_high_offset_xxlarge.psd" \
            "normal_rle_high_offset_xxlarge.psd" \
            "normal_high_offset_xxlarge_layer_info_end_overrun.psd" \
            "normal_high_offset_xxlarge_channel_window_overrun.psd" \
            "normal_rle_high_offset_xxlarge_rle_payload_window_overrun.psd"; do
            test -f "${dst_formats_dir}/${base}_${suffix}" || {
                fixtures_ready=0
                break 3
            }
        done
    done
done

if [ "${fixtures_ready}" -eq 1 ] && [ -f "${webp_padded}" ]; then
    exit 0
fi

printf '%s\n' "prepare-psd-large-fixtures: extracting packaged large fixtures"

for mode_prefix in cmyk mode7_cmyk; do
    for depth_tag in 16 32; do
        base="snake16_psb_${mode_prefix}${depth_tag}_missing_composite_multilayer"
        for suffix in \
            "normal_high_offset_xxlarge.psd" \
            "normal_rle_high_offset_xxlarge.psd" \
            "normal_high_offset_xxlarge_layer_info_end_overrun.psd" \
            "normal_high_offset_xxlarge_channel_window_overrun.psd" \
            "normal_rle_high_offset_xxlarge_rle_payload_window_overrun.psd"; do
            fixture="${base}_${suffix}"
            dst_file="${dst_formats_dir}/${fixture}"
            src_file_gz="${src_formats_dir}/${fixture}.gz"
            test -f "${dst_file}" && continue
            test -f "${src_file_gz}" || {
                printf '%s\n' "error: missing packaged fixture: ${src_file_gz}" >&2
                exit 1
            }
            gzip -dc "${src_file_gz}" > "${dst_file}"
        done
    done
done

test -f "${webp_padded}" || {
    test -f "${webp_padded_gz}" || {
        printf '%s\n' "error: missing packaged fixture: ${webp_padded_gz}" >&2
        exit 1
    }
    gzip -dc "${webp_padded_gz}" > "${webp_padded}"
}

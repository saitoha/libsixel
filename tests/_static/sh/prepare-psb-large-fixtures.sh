#!/bin/sh

set -eu

if [ "$#" -ne 1 ]; then
    printf '%s\n' "usage: $0 TOP_SRCDIR" >&2
    exit 2
fi

top_srcdir=$1
formats_dir="${top_srcdir}/tests/data/inputs/formats"
generator="${formats_dir}/generate_psb_missing_composite_fixtures.py"

if [ ! -f "${generator}" ]; then
    printf '%s\n' "error: missing generator: ${generator}" >&2
    exit 1
fi

webp_padded="${formats_dir}/webp-static-icc-overlimit-padded.webp"
webp_padded_gz="${webp_padded}.gz"
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
            if [ ! -f "${formats_dir}/${base}_${suffix}" ]; then
                fixtures_ready=0
                break 3
            fi
        done
    done
done

if [ "${fixtures_ready}" -eq 1 ] && [ -f "${webp_padded}" ]; then
    exit 0
fi

python_bin=${PYTHON:-python3}
if ! command -v "${python_bin}" >/dev/null 2>&1; then
    if command -v python3 >/dev/null 2>&1; then
        python_bin=python3
    elif command -v python >/dev/null 2>&1; then
        python_bin=python
    else
        printf '%s\n' "error: python interpreter not found (need python3/python)" >&2
        exit 1
    fi
fi

printf '%s\n' "prepare-psd-large-fixtures: generating missing large PSB fixtures"
"${python_bin}" "${generator}" --high-offset-over1m-only

if [ ! -f "${webp_padded}" ]; then
    if [ ! -f "${webp_padded_gz}" ]; then
        printf '%s\n' "error: missing compressed fixture: ${webp_padded_gz}" >&2
        exit 1
    fi
    gzip -dc "${webp_padded_gz}" > "${webp_padded}"
fi

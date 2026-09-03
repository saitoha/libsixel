#!/bin/sh
# Measure the end-to-end quality effect of 8-bit lookup policies.

set -eu

TOP_SRCDIR=${TOP_SRCDIR-${0%/*}/..}
PYTHON=${PYTHON-python3}
IMG2SIXEL_PATH=${IMG2SIXEL_PATH-${TOP_SRCDIR}/converters/img2sixel}
LSQA_PATH=${LSQA_PATH-${TOP_SRCDIR}/assessment/lsqa}

output_dir=${1-.}
input_image=${2-${TOP_SRCDIR}/images/snake.png}
input_name=${input_image##*/}

mkdir -p "${output_dir}"

common='{img2sixel} --threads=1 --precision=8bit --quality=full --quantize-model=heckbert:cover=off:merge=none --diffusion=none'

plot_quality_curve()
{
    "${PYTHON}" "${TOP_SRCDIR}/tools/plot_quality_curve.py" \
        "${input_image}" \
        --colors 8,16,32,64,128,256 \
        --jobs 1 \
        --command1 "${common} --lookup-policy=none -p {ncolors}" \
        --label1 none \
        --command2 "${common} --lookup-policy=5bit -p {ncolors}" \
        --label2 5bit \
        --command3 "${common} --lookup-policy=6bit -p {ncolors}" \
        --label3 6bit \
        --command4 "${common} --lookup-policy=certlut -p {ncolors}" \
        --label4 certlut \
        --command5 "${common} --lookup-policy=eytzinger -p {ncolors}" \
        --label5 eytzinger \
        --command6 "${common} --lookup-policy=vptree -p {ncolors}" \
        --label6 vptree \
        --img2sixel "${IMG2SIXEL_PATH}" \
        --lsqa "${LSQA_PATH}" \
        "$@"
}

plot_quality_curve \
    --metrics 'Δ E00_mean,Δ Chroma_mean' \
    --output-csv "${output_dir}/lookup-policy-color-error.csv" \
    --output-plot "${output_dir}/lookup-policy-color-error.png" \
    --title "Legacy five-bit binning raises high-K color error on ${input_name}"

plot_quality_curve \
    --metrics MS-SSIM \
    --output-csv "${output_dir}/lookup-policy-ms-ssim.csv" \
    --output-plot "${output_dir}/lookup-policy-ms-ssim.png" \
    --title "Perceptual effect of legacy five-bit binning on ${input_name}"

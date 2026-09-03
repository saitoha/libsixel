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

heckbert_common='{img2sixel} --threads=1 --precision=8bit --quality=full --quantize-model=heckbert:cover=off:merge=none --diffusion=none --gpu-policy=off'
modern_common='{img2sixel} --threads=1 --precision=8bit --quality=full -Qkmeans:Gw -Xoklab -Wgamma --diffusion=none --gpu-policy=off'

plot_quality_curve()
{
    policy_colors=$1
    policy_common=$2
    shift 2
    "${PYTHON}" "${TOP_SRCDIR}/tools/plot_quality_curve.py" \
        "${input_image}" \
        --colors "${policy_colors}" \
        --jobs 1 \
        --command1 "${policy_common} --lookup-policy=none {input}" \
        --label1 none \
        --command2 "${policy_common} --lookup-policy=5bit {input}" \
        --label2 5bit \
        --command3 "${policy_common} --lookup-policy=6bit {input}" \
        --label3 6bit \
        --command4 "${policy_common} --lookup-policy=certlut {input}" \
        --label4 certlut \
        --command5 "${policy_common} --lookup-policy=eytzinger {input}" \
        --label5 eytzinger \
        --command6 "${policy_common} --lookup-policy=fhedt {input}" \
        --label6 fhedt \
        --command7 "${policy_common} --lookup-policy=vptree {input}" \
        --label7 vptree \
        --command8 "${policy_common} --lookup-policy=rbc {input}" \
        --label8 rbc \
        --command9 "${policy_common} --lookup-policy=mahalanobis {input}" \
        --label9 mahalanobis \
        --img2sixel "${IMG2SIXEL_PATH}" \
        --lsqa "${LSQA_PATH}" \
        "$@"
}

plot_quality_curve \
    8,16,32,64,128,256 \
    "${heckbert_common} -p {ncolors}" \
    --metrics 'Δ E00_mean,Δ Chroma_mean' \
    --output-csv "${output_dir}/lookup-policy-color-error.csv" \
    --output-plot "${output_dir}/lookup-policy-color-error.png" \
    --title "Current Heckbert compatibility comparison on ${input_name}"

plot_quality_curve \
    8,16,32,64,128,256 \
    "${heckbert_common} -p {ncolors}" \
    --metrics MS-SSIM \
    --output-csv "${output_dir}/lookup-policy-ms-ssim.csv" \
    --output-plot "${output_dir}/lookup-policy-ms-ssim.png" \
    --title "Current Heckbert compatibility MS-SSIM on ${input_name}"

plot_quality_curve \
    8,16,32,64,128,256 \
    "${modern_common} -p {ncolors}" \
    --metrics 'Δ E00_mean,Δ Chroma_mean' \
    --output-csv "${output_dir}/lookup-policy-kmeans-color-error.csv" \
    --output-plot "${output_dir}/lookup-policy-kmeans-color-error.png" \
    --title "K-means lookup-policy color error on ${input_name}"

plot_quality_curve \
    8,16,32,64,128,256 \
    "${modern_common} -p {ncolors}" \
    --metrics MS-SSIM \
    --output-csv "${output_dir}/lookup-policy-kmeans-ms-ssim.csv" \
    --output-plot "${output_dir}/lookup-policy-kmeans-ms-ssim.png" \
    --title "K-means lookup-policy MS-SSIM on ${input_name}"

plot_quality_curve \
    128,144,160,176,192,208,224,240,256 \
    "${modern_common} -p {ncolors}" \
    --metrics 'Δ E00_mean,Δ Chroma_mean' \
    --output-csv "${output_dir}/lookup-policy-kmeans-high-k.csv" \
    --output-plot "${output_dir}/lookup-policy-kmeans-high-k.png" \
    --title "K-means lookup-policy color error, K=128–256, on ${input_name}"

plot_quality_curve \
    128,144,160,176,192,208,224,240,256 \
    "${heckbert_common} -p {ncolors}" \
    --metrics 'Δ E00_mean,Δ Chroma_mean' \
    --output-csv "${output_dir}/lookup-policy-heckbert-high-k.csv" \
    --output-plot "${output_dir}/lookup-policy-heckbert-high-k.png" \
    --title "Heckbert palette-geometry sensitivity, K=128–256, on ${input_name}"

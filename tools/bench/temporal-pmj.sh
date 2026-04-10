#!/bin/sh
# SPDX-License-Identifier: MIT
#
# Temporal PMJ micro benchmark for local optimization checks.
# Output format:
#   strategy<TAB>precision<TAB>threads<TAB>elapsed_ms

set -eu

script_dir=${0%/*}
repo_root=$(cd "${script_dir}/../.." && pwd)

img2sixel_bin=${IMG2SIXEL_BIN:-"${repo_root}/build-autotools-temporalcheck/converters/img2sixel"}
input_image=${1:-"${repo_root}/images/snake.png"}
runs=${2:-3}
output_dir="${repo_root}/.artifacts"
output_file="${output_dir}/temporal-pmj.tsv"

if ! test -x "${img2sixel_bin}"; then
    echo "img2sixel binary is not executable: ${img2sixel_bin}" >&2
    exit 1
fi
if ! test -f "${input_image}"; then
    echo "input image not found: ${input_image}" >&2
    exit 1
fi

mkdir -p "${output_dir}"
printf "strategy\tprecision\tthreads\telapsed_ms\n" > "${output_file}"

for precision in 8bit float32; do
    for threads in 1 2; do
        for strategy in diffusion stbn-hash stbn-mask pmj; do
            run=1
            while test "${run}" -le "${runs}"; do
                start_ms=$(
                    perl -MTime::HiRes=time -e 'printf("%.0f\n", time()*1000)'
                )
                if test "${precision}" = "float32"; then
                    "${img2sixel_bin}" \
                        -d "temporal-diffusion:strategy=${strategy}" \
                        --precision=float32 \
                        -p 16 \
                        --threads="${threads}" \
                        "${input_image}" >/dev/null
                else
                    "${img2sixel_bin}" \
                        -d "temporal-diffusion:strategy=${strategy}" \
                        -p 16 \
                        --threads="${threads}" \
                        "${input_image}" >/dev/null
                fi
                end_ms=$(
                    perl -MTime::HiRes=time -e 'printf("%.0f\n", time()*1000)'
                )
                elapsed_ms=$((end_ms - start_ms))

                printf "%s\t%s\t%s\t%s\n" \
                    "${strategy}" \
                    "${precision}" \
                    "${threads}" \
                    "${elapsed_ms}" >> "${output_file}"
                run=$((run + 1))
            done
        done
    done
done

printf "Wrote %s\n" "${output_file}"

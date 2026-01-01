#!/bin/sh
# quality.sh - Thin wrapper to compare images with assessment/lsqa.
#
# The helper locates the built lsqa binary (Autotools or Meson), runs it on
# a reference and output image pair, parses MS-SSIM and PSNR_Y from the JSON
# report, and enforces caller-provided thresholds.

# Enable trace if caller set -x; preserve -e/-u behavior chosen by caller.
quality_initial_flags=$-
set -x
case ${quality_initial_flags} in
*e*) set -e ;;
esac
case ${quality_initial_flags} in
*u*) set -u ;;
esac

quality_script_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
quality_root=$(CDPATH=; cd "${quality_script_dir}/.." && pwd)

if [ -n "${MESON_BUILD_ROOT:-}" ]; then
    quality_build_root=${TOP_BUILDDIR:-${MESON_BUILD_ROOT}}
else
    quality_build_root=${TOP_BUILDDIR:-${quality_root}/..}
fi

if [ -z "${LSQA_BIN:-}" ]; then
    for quality_candidate in \
            "${quality_build_root}/assessment/lsqa" \
            "${quality_build_root}/lsqa"; do
        if [ -x "${quality_candidate}" ]; then
            LSQA_BIN=${quality_candidate}
            break
        fi
    done
fi

quality_require_lsqa() {
    if [ -z "${LSQA_BIN:-}" ] || [ ! -x "${LSQA_BIN}" ]; then
        skip_all "assessment/lsqa binary is unavailable"
    fi
}

quality_compare_images() {
    ref_image=$1
    out_image=$2
    min_ssim=$3
    min_psnr=$4
    log_file=$5

    quality_tmp=${TMPDIR:-/tmp}/lsqa_quality.$$

    if ! "${LSQA_BIN}" "${ref_image}" "${out_image}" \
            >"${quality_tmp}" 2>>"${log_file}"; then
        rm -f "${quality_tmp}"
        return 1
    fi

    python3 - "$quality_tmp" "$min_ssim" "$min_psnr" "$log_file" <<'PY'
import json
import sys

report_path, min_ssim, min_psnr, log_path = sys.argv[1:5]
min_ssim = float(min_ssim)
min_psnr = float(min_psnr)

with open(report_path, 'r', encoding='utf-8') as handle:
    data = json.load(handle)

quality = data.get('quality', {})
ssim = quality.get('MS-SSIM') or 0.0
psnr = quality.get('PSNR_Y') or 0.0

message = f"MS-SSIM={ssim:.6f}, PSNR_Y={psnr:.2f}dB"
with open(log_path, 'a', encoding='utf-8') as handle:
    handle.write(message + "\n")

if ssim + 1e-9 < min_ssim or psnr + 1e-9 < min_psnr:
    sys.exit(1)

PY
    status=$?
    rm -f "${quality_tmp}"
    return ${status}
}

#!/bin/sh

set -e

original="${1}"
[ -n "${original}" ]

sixel="${2--}"
[ -n "${sixel}" ]

dir="$(cd "$(dirname "${0}")" && pwd)"
src_topdir="$(cd "${dir}/.." && pwd)"
imgname="$(basename "${sixel}")"
prefix="imagemagick-${imgname}"
metrics_path="${prefix}_metrics.json"
scores_path="${prefix}_scores.json"
spectrum_path="${prefix}_spectrum.png"
radar_path="${prefix}_radar_scores.png"
html_path="${prefix}.html"

tmp_png="$(mktemp "${TMPDIR:-/tmp}/lsqa.XXXXXX.png")"

cleanup() {
    rm -f "${tmp_png}"
}

trap cleanup EXIT INT TERM

"${src_topdir}/converters/sixel2png" "${sixel}" > "${tmp_png}"

metrics_json="$(LSQA_PREFIX="${prefix}" "${src_topdir}/assessment/lsqa" \
    "${original}" \
    "${tmp_png}")"

histogram_json="$("${src_topdir}/tools/evaluate_histogram.py" \
    --ref "${original}" \
    --out "${tmp_png}" \
    --output "${spectrum_path}")"

printf '%s\n' "${metrics_json}" |
"${src_topdir}/tools/evaluate_scores.py" \
    --output "${scores_path}" \
    --radar-output "${radar_path}"

scores_json="$(cat "${scores_path}")"

printf '%s\n' \
    "${metrics_json}" \
    "${histogram_json}" \
    "${scores_json}" |
"${src_topdir}/tools/evaluate_report.py" \
    --output "${html_path}"

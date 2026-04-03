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

expected_count=158
current_count=0
webp_padded="${formats_dir}/webp-static-icc-overlimit-padded.webp"
webp_padded_gz="${webp_padded}.gz"
for path in \
    "${formats_dir}"/snake16_psb_*_high_offset_*large*.psd; do
    if [ -f "${path}" ]; then
        current_count=$((current_count + 1))
    fi
done

if [ "${current_count}" -ge "${expected_count}" ] && [ -f "${webp_padded}" ]; then
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

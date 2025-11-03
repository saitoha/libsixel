#!/usr/bin/env bash
# Validate sixel2png behaviour.
set -euo pipefail
SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
# shellcheck source=tests/converters/common.bash
source "${SCRIPT_DIR}/common.bash"

echo '[test11] sixel2png'

for name in snake.six map8.six map64.six; do
    require_file "${IMAGES_DIR}/${name}"
done

expect_failure() {
    local output_file

    output_file="${TMP_DIR}/capture.$$"
    if run_sixel2png "$@" >"${output_file}" 2>/dev/null; then
        :
    fi
    if [[ -s ${output_file} ]]; then
        printf 'sixel2png unexpectedly produced output: %s\n' "$*" >&2
        rm -f "${output_file}"
        exit 1
    fi
    rm -f "${output_file}"
}

# Reject nonexistent Sixel input file and surface suggestions.
missing_err="${TMP_DIR}/sixel2png-missing.err"
rm -f "${missing_err}"
if run_sixel2png -i "${TMP_DIR}/unknown.six" \
        >"${TMP_DIR}/capture.$$" 2>"${missing_err}"; then
    echo 'sixel2png unexpectedly accepted missing input path' >&2
    rm -f "${missing_err}" "${TMP_DIR}/capture.$$"
    exit 1
fi
if ! grep -F 'sixel2png: input file' "${missing_err}" >/dev/null; then
    echo 'missing input diagnostic' >&2
    cat "${missing_err}" >&2 || :
    rm -f "${missing_err}" "${TMP_DIR}/capture.$$"
    exit 1
fi
rm -f "${missing_err}" "${TMP_DIR}/capture.$$"

# Provide a local SIXEL sample for option regression tests.
cp "${IMAGES_DIR}/snake.six" "${TMP_DIR}/snake.six"
output_file="${TMP_DIR}/capture.$$"
# Ensure invalid legacy width syntax is ignored.
if run_sixel2png -% < "${TMP_DIR}/snake.six" >"${output_file}" 2>/dev/null; then
    :
fi
if [[ -s ${output_file} ]]; then
    echo 'sixel2png unexpectedly produced output for -%' >&2
    rm -f "${output_file}"
    exit 1
fi
rm -f "${output_file}"
output_file="${TMP_DIR}/capture.$$"
# Ensure invalid output filename is rejected.
if run_sixel2png invalid_filename < "${IMAGES_DIR}/snake.six" >"${output_file}" 2>/dev/null; then
    :
fi
if [[ -s ${output_file} ]]; then
    echo 'sixel2png unexpectedly produced output for invalid filename' >&2
    rm -f "${output_file}"
    exit 1
fi
rm -f "${output_file}"

# Confirm help output is accessible.
run_sixel2png -H
# Confirm version output is accessible.
run_sixel2png -V
# Convert Sixel snake to PNG via stdin.
run_sixel2png < "${IMAGES_DIR}/snake.six" > "${TMP_DIR}/snake1.png"
# Convert Sixel map8 to PNG via stdin.
run_sixel2png < "${IMAGES_DIR}/map8.six" > "${TMP_DIR}/map8.png"
# Convert Sixel map64 to PNG using explicit stdin/stdout markers.
run_sixel2png - - < "${IMAGES_DIR}/map64.six" > "${TMP_DIR}/map64.png"
# Convert Sixel snake to PNG using file arguments.
run_sixel2png -i "${IMAGES_DIR}/snake.six" -o "${TMP_DIR}/snake4.png"

# Ensure prefixed PNG targets create directories and strip the scheme.
prefixed_dir="${TMP_DIR}/s2p-prefix"
rm -rf "${prefixed_dir}"
mkdir -p "${prefixed_dir}"
run_sixel2png -o "png:${prefixed_dir}/out.png" \
    < "${IMAGES_DIR}/snake.six"
if [[ ! -s "${prefixed_dir}/out.png" ]]; then
    echo 'prefixed PNG output missing' >&2
    exit 1
fi

# Confirm png:- writes to stdout by checking pipeline success.
run_sixel2png -o "png:-" < "${IMAGES_DIR}/snake.six" \
    >"${TMP_DIR}/png-stdout.png"

# Reject empty png: payloads with a clear diagnostic.
png_err="${TMP_DIR}/sixel2png-png.err"
if run_sixel2png -o "png:" < "${IMAGES_DIR}/snake.six" \
        >"${TMP_DIR}/capture.$$" 2>"${png_err}"; then
    echo 'sixel2png accepted empty png: payload' >&2
    rm -f "${png_err}" "${TMP_DIR}/capture.$$"
    exit 1
fi
if ! grep -F 'missing target after the "png:" prefix' "${png_err}" >/dev/null;
then
    echo 'missing png: diagnostic' >&2
    cat "${png_err}" >&2 || :
    rm -f "${png_err}" "${TMP_DIR}/capture.$$"
    exit 1
fi
rm -f "${png_err}" "${TMP_DIR}/capture.$$"

# Validate unique prefix handling for the dequantizer toggle.
run_sixel2png -dn < "${IMAGES_DIR}/snake.six" >/dev/null
ambiguous_err="${TMP_DIR}/sixel2png-ambiguous.err"
if run_sixel2png -dk_ < "${IMAGES_DIR}/snake.six" \
        >"${TMP_DIR}/capture.$$" 2>"${ambiguous_err}"; then
    echo 'sixel2png accepted ambiguous -dk_' >&2
    rm -f "${ambiguous_err}" "${TMP_DIR}/capture.$$"
    exit 1
fi
if ! grep -F 'ambiguous prefix "k_" for --dequantize' "${ambiguous_err}" \
        >/dev/null; then
    echo 'missing ambiguous prefix diagnostic' >&2
    cat "${ambiguous_err}" >&2 || :
    rm -f "${ambiguous_err}" "${TMP_DIR}/capture.$$"
    exit 1
fi
rm -f "${ambiguous_err}" "${TMP_DIR}/capture.$$"

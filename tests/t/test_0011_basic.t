#!/bin/sh
# TAP test validating sixel2png CLI behaviours and regressions.

# Enable strict mode with verbose tracing for diagnostics.
set -euxv

test_name=$(basename "$0")
artifact_root=${ARTIFACT_ROOT:-"$(pwd)/_artifacts"}
artifact_dir="${artifact_root}/${test_name}"
log_file="${artifact_dir}/sixel2png.log"
output_dir="${artifact_dir}/outputs"
tmp_dir="${artifact_dir}/tmp"

mkdir -p "${output_dir}" "${tmp_dir}"

script_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
. "${script_dir}/converters-common.t"

status=0
case_id=1

ensure_converter_available "SIXEL2PNG" "${SIXEL2PNG_PATH}" "sixel2png"

pass() {
    printf 'ok %s - %s\n' "$1" "$2"
}

fail() {
    printf 'not ok %s - %s\n' "$1" "$2"
    status=1
}

skip() {
    reason="skipped"
    if [ "$#" -ge 3 ]; then
        reason=$3
    fi

    printf 'ok %s - %s # SKIP %s\n' "$1" "$2" "${reason}"
}

echo "1..17"

require_file "${images_dir}/snake.six"
require_file "${images_dir}/map8.six"
require_file "${images_dir}/map64.six"

missing_capture=$(make_temp_file "${tmp_dir}" "sixel2png-missing")
missing_err=$(make_temp_file "${tmp_dir}" "sixel2png-missing-err")
if run_sixel2png -i "${tmp_dir}/unknown.six" \
        >"${missing_capture}" 2>"${missing_err}"; then
    fail ${case_id} "accepts missing input path"
else
    pass ${case_id} "rejects missing input path"
fi
rm -f "${missing_capture}" "${missing_err}"
case_id=$((case_id + 1))

if run_sixel2png -H >"${output_dir}/help.txt" 2>>"${log_file}"; then
    pass ${case_id} "prints help"
else
    fail ${case_id} "help option failed"
fi
case_id=$((case_id + 1))

if run_sixel2png -V >"${output_dir}/version.txt" 2>>"${log_file}"; then
    pass ${case_id} "prints version"
else
    fail ${case_id} "version option failed"
fi
case_id=$((case_id + 1))

if run_sixel2png <"${images_dir}/snake.six" \
        >"${output_dir}/snake-stdin.png" 2>>"${log_file}"; then
    pass ${case_id} "converts snake from stdin"
else
    fail ${case_id} "snake stdin conversion failed"
fi
case_id=$((case_id + 1))

if run_sixel2png <"${images_dir}/map8.six" \
        >"${output_dir}/map8-stdin.png" 2>>"${log_file}"; then
    pass ${case_id} "converts map8 from stdin"
else
    fail ${case_id} "map8 stdin conversion failed"
fi
case_id=$((case_id + 1))

if run_sixel2png - - <"${images_dir}/map64.six" \
        >"${output_dir}/map64-stdin-stdout.png" 2>>"${log_file}"; then
    pass ${case_id} "converts map64 with explicit stdin/stdout"
else
    fail ${case_id} "map64 stdin/stdout conversion failed"
fi
case_id=$((case_id + 1))

if run_sixel2png -i "${images_dir}/snake.six" \
        -o "${output_dir}/snake-file.png" 2>>"${log_file}"; then
    pass ${case_id} "converts snake with file arguments"
else
    fail ${case_id} "snake file conversion failed"
fi
case_id=$((case_id + 1))

direct_png="${output_dir}/snake-direct.png"
if run_sixel2png -D <"${images_dir}/snake.six" \
        >"${direct_png}" 2>>"${log_file}"; then
    pass ${case_id} "produces direct RGBA output"
else
    fail ${case_id} "direct RGBA conversion failed"
fi
case_id=$((case_id + 1))

direct_err=$(make_temp_file "${tmp_dir}" "sixel2png-direct-err")
if run_sixel2png -D -dk_undither <"${images_dir}/snake.six" \
        >"${tmp_dir}/capture.$$" 2>"${direct_err}"; then
    fail ${case_id} "accepts conflicting direct/dequantize flags"
else
    if grep -F "cannot be combined" "${direct_err}" >/dev/null; then
        pass ${case_id} "rejects direct/dequantize mix"
    else
        fail ${case_id} "missing direct/dequantize diagnostic"
    fi
fi
rm -f "${direct_err}" "${tmp_dir}/capture.$$"
case_id=$((case_id + 1))

prefixed_dir="${output_dir}/s2p-prefix"
rm -rf "${prefixed_dir}"
mkdir -p "${prefixed_dir}"
if run_sixel2png -o "png:${prefixed_dir}/out.png" \
        <"${images_dir}/snake.six" 2>>"${log_file}"; then
    if [ -s "${prefixed_dir}/out.png" ]; then
        pass ${case_id} "prefixed output trims scheme"
    else
        fail ${case_id} "prefixed output missing"
    fi
else
    fail ${case_id} "prefixed output command failed"
fi
case_id=$((case_id + 1))

png_stdout="${output_dir}/png-stdout.png"
if run_sixel2png -o "png:-" <"${images_dir}/snake.six" \
        >"${png_stdout}" 2>>"${log_file}"; then
    if [ -s "${png_stdout}" ]; then
        pass ${case_id} "png:- writes to stdout"
    else
        fail ${case_id} "png:- produced empty output"
    fi
else
    fail ${case_id} "png:- command failed"
fi
case_id=$((case_id + 1))

png_err=$(make_temp_file "${tmp_dir}" "sixel2png-png-err")
if run_sixel2png -o "png:" <"${images_dir}/snake.six" \
        >"${tmp_dir}/capture.$$" 2>"${png_err}"; then
    fail ${case_id} "accepts empty png: prefix"
else
    if grep -F 'missing target after the "png:" prefix' "${png_err}" >/dev/null; then
        pass ${case_id} "rejects empty png prefix"
    else
        fail ${case_id} "missing png prefix diagnostic"
    fi
fi
rm -f "${png_err}" "${tmp_dir}/capture.$$"
case_id=$((case_id + 1))

ambiguous_err=$(make_temp_file "${tmp_dir}" "sixel2png-ambiguous")
set +xv
if run_sixel2png -dk_ <"${images_dir}/snake.six" \
        >"${output_dir}/dequantize-short.png" 2>"${ambiguous_err}"; then
    set -xv
    if [ ! -s "${ambiguous_err}" ]; then
        pass ${case_id} "accepts unique dequantize prefix"
    else
        fail ${case_id} "unexpected diagnostics for -dk_"
    fi
else
    set -xv
    fail ${case_id} "unique dequantize prefix rejected"
fi
rm -f "${ambiguous_err}"
case_id=$((case_id + 1))

comparator_cmd=""
if command -v cmp >/dev/null 2>&1; then
    comparator_cmd="cmp -s"
elif command -v diff >/dev/null 2>&1; then
    comparator_cmd="diff -q"
fi

files_identical() {
    if [ -z "${comparator_cmd}" ]; then
        return 1
    fi

    if ${comparator_cmd} "$1" "$2"; then
        return 0
    fi
    return 1
}

if [ -z "${comparator_cmd}" ]; then
    skip ${case_id} "cmp/diff unavailable"
    skip $((case_id + 1)) "cmp/diff unavailable"
    skip $((case_id + 2)) "cmp/diff unavailable"
    case_id=$((case_id + 3))
else
    parallel_indexed_1="${output_dir}/parallel-indexed-1.png"
    parallel_indexed_4="${output_dir}/parallel-indexed-4.png"
    SIXEL_THREADS=1 run_sixel2png \
        <"${images_dir}/map64.six" \
        >"${parallel_indexed_1}" 2>>"${log_file}"
    SIXEL_THREADS=4 run_sixel2png \
        <"${images_dir}/map64.six" \
        >"${parallel_indexed_4}" 2>>"${log_file}"
    if files_identical "${parallel_indexed_1}" "${parallel_indexed_4}"; then
        pass ${case_id} "parallel indexed matches serial"
    else
        fail ${case_id} "parallel indexed diverges"
    fi
    case_id=$((case_id + 1))

    parallel_direct_1="${output_dir}/parallel-direct-1.png"
    parallel_direct_4="${output_dir}/parallel-direct-4.png"
    SIXEL_THREADS=1 run_sixel2png -D \
        <"${images_dir}/map64.six" \
        >"${parallel_direct_1}" 2>>"${log_file}"
    SIXEL_THREADS=4 run_sixel2png -D \
        <"${images_dir}/map64.six" \
        >"${parallel_direct_4}" 2>>"${log_file}"
    if files_identical "${parallel_direct_1}" "${parallel_direct_4}"; then
        pass ${case_id} "parallel direct matches serial"
    else
        fail ${case_id} "parallel direct diverges"
    fi
    case_id=$((case_id + 1))

    parallel_direct_cli="${output_dir}/parallel-direct-cli.png"
    SIXEL_THREADS=1 run_sixel2png -D \
        <"${images_dir}/map64.six" \
        >"${parallel_direct_cli}" 2>>"${log_file}"
    if files_identical "${parallel_direct_cli}" "${parallel_direct_4}"; then
        pass ${case_id} "CLI thread override matches env"
    else
        fail ${case_id} "CLI thread override diverges"
    fi
    case_id=$((case_id + 1))
fi

threads_err=$(make_temp_file "${tmp_dir}" "sixel2png-threads-err")
if run_sixel2png -= bogus <"${images_dir}/map64.six" \
        >"${tmp_dir}/capture.$$" 2>"${threads_err}"; then
    fail ${case_id} "accepts invalid thread token"
else
    if grep -F "threads must be a positive integer or 'auto'" \
            "${threads_err}" >/dev/null; then
        pass ${case_id} "rejects invalid thread token"
    else
        fail ${case_id} "missing invalid thread diagnostic"
    fi
fi
rm -f "${threads_err}" "${tmp_dir}/capture.$$"

exit "${status}"

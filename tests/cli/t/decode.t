#!/bin/sh
# TAP test to validate decoding workflow with artifact logging.

# Enable strict mode with verbose tracing for diagnostics.
set -uxv

test_name=$(basename "$0")
artifact_root=${ARTIFACT_ROOT:-"$(pwd)/_artifacts"}
artifact_dir="${artifact_root}/${test_name}"
log_file="${artifact_dir}/decode.log"

mkdir -p "$artifact_dir"

status=0

pass() {
    printf 'ok %s - %s\n' "$1" "$2"
}

fail() {
    printf 'not ok %s - %s\n' "$1" "$2"
    status=1
}

echo "1..3"

if [ -d "$artifact_dir" ]; then
    pass 1 "artifact directory is ready"
else
    fail 1 "artifact directory is missing"
fi

encoded_content=$(printf 'sixel decode sample\n' | base64)
encoded_file="${artifact_dir}/encoded.txt"
decoded_file="${artifact_dir}/decoded.txt"
printf '%s' "$encoded_content" > "$encoded_file"

if command -v base64 >/dev/null 2>&1; then
    if base64 -d "$encoded_file" > "$decoded_file" 2>>"$log_file"; then
        pass 2 "decoded sample text with base64"
    else
        fail 2 "base64 command failed"
    fi
else
    echo "base64 utility is unavailable" >> "$log_file"
    fail 2 "base64 utility is unavailable"
fi

expected_plain='sixel decode sample'
if [ -s "$decoded_file" ] && [ "$(cat "$decoded_file" 2>/dev/null)" = "${expected_plain}" ]; then
    pass 3 "decoded content matches expected value"
else
    fail 3 "decoded content mismatch"
fi

exit "$status"

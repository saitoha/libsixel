#!/bin/sh
# TAP test to validate encoding workflow and artifact collection.

# Enable strict mode with verbose tracing for diagnostics.
set -uxv

test_name=$(basename "$0")
artifact_root=${ARTIFACT_ROOT:-"$(pwd)/_artifacts"}
artifact_dir="${artifact_root}/${test_name}"
log_file="${artifact_dir}/encode.log"

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

input_file="${artifact_dir}/input.txt"
encoded_file="${artifact_dir}/encoded.txt"
printf 'sixel encode sample\n' > "$input_file"

if command -v base64 >/dev/null 2>&1; then
    if base64 "$input_file" > "$encoded_file" 2>>"$log_file"; then
        pass 2 "encoded sample text with base64"
    else
        fail 2 "base64 command failed"
    fi
else
    echo "base64 utility is unavailable" >> "$log_file"
    fail 2 "base64 utility is unavailable"
fi

expected_content=$(printf 'sixel encode sample\n' | base64)
if [ -s "$encoded_file" ] && [ "$(cat "$encoded_file" 2>/dev/null)" = "$expected_content" ]; then
    pass 3 "encoded content matches expected value"
else
    fail 3 "encoded content mismatch"
fi

exit "$status"

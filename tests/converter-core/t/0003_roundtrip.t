#!/bin/sh
# TAP test to validate roundtrip encode/decode behavior and artifact storage.

# Enable strict mode with verbose tracing for diagnostics.
set -uxv

test_name=$(basename "$0")
artifact_root=${ARTIFACT_ROOT:-"$(pwd)/_artifacts"}
artifact_dir="${artifact_root}/${test_name}"
log_file="${artifact_dir}/roundtrip.log"

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

input_file="${artifact_dir}/0040_roundtrip.txt"
encoded_file="${artifact_dir}/roundtrip.b64"
decoded_file="${artifact_dir}/roundtrip.out"
printf 'sixel roundtrip sample\n' > "$input_file"

if command -v base64 >/dev/null 2>&1; then
    if base64 "$input_file" > "$encoded_file" 2>>"$log_file"; then
        pass 2 "encoded sample text for roundtrip"
    else
        fail 2 "encoding step failed"
    fi
else
    echo "base64 utility is unavailable" >> "$log_file"
    fail 2 "base64 utility is unavailable"
fi

if command -v base64 >/dev/null 2>&1 && base64 -d "$encoded_file" > "$decoded_file" 2>>"$log_file"; then
    :
else
    fail 3 "decoding step failed"
fi

if [ -s "$decoded_file" ] && diff -u "$input_file" "$decoded_file" >>"$log_file" 2>&1; then
    pass 3 "roundtrip data matches original"
else
    fail 3 "roundtrip data mismatch"
fi

exit "$status"

#!/bin/sh
# Verify source strategy tokens against declared test uses and registry names.
# Behavioral assertions and coverage ownership live in the dithering policy;
# this check does not prescribe test filenames or assertion implementation.
# Policy: docs/encoder/palette-application/dithering/README.md

set -eu

echo "1..1"

src_root=$1
source_file_h=$src_root/src/dither-interframe-method.h
source_file_c=$src_root/src/dither-interframe-method.c
registry_file=$src_root/src/options-registry.c
interframe_tests_dir=$src_root/tests/processing/dither/interframe
status=0

for required in "$source_file_h" "$source_file_c" "$registry_file"; do
    test -f "$required" || {
        echo "not ok 1 - interframe strategy tokens stay synchronized"
        echo "# missing source: $required"
        exit 1
    }
done
test -d "$interframe_tests_dir" || {
    echo "not ok 1 - interframe strategy tokens stay synchronized"
    echo "# missing test directory: $interframe_tests_dir"
    exit 1
}

# The parser is authoritative. Strategy-token comments identify positive
# compatibility evidence; unknown-value/fallback specimens are not tokens.
# Check both directions, and require a real, exact assignment in the marked
# test so a comment or a longer token prefix cannot satisfy source coverage.
awk '
FILENAME == ARGV[1] || FILENAME == ARGV[2] {
    if (/^sixel_interframe_strategy_token_from_string\(/) in_func = 1
    if (in_func && /strcmp\(value, "/) {
        token = $0
        sub(/.*strcmp\(value, "/, "", token)
        sub(/".*/, "", token)
        source[token] = 1
        source_count++
    }
    if (/^}/) in_func = 0
    next
}
/^# Strategy-token: [a-z0-9-]+$/ {
    declared[$3] = 1
    owners[FILENAME, $3] = 1
    next
}
/^[[:space:]]*#/ { next }
{
    line = $0
    while (match(line, /(^|[[:space:]])SIXEL_DITHER_STBN_SOURCE=[A-Za-z0-9_-]+([[:space:];]|$)/)) {
        token = substr(line, RSTART, RLENGTH)
        sub(/^[[:space:]]*SIXEL_DITHER_STBN_SOURCE=/, "", token)
        sub(/[[:space:];]$/, "", token)
        used[FILENAME, token] = 1
        line = substr(line, RSTART + RLENGTH)
    }
}
END {
    if (!source_count) {
        print "# strategy parser has no tokens"
        failed = 1
    }
    for (token in source) {
        if (!(token in declared)) {
            print "# missing strategy test declaration: " token
            failed = 1
        }
    }
    for (token in declared) {
        if (!(token in source)) {
            print "# test declares a strategy absent from source: " token
            failed = 1
        }
    }
    for (owner in owners) {
        if (!(owner in used)) {
            split(owner, parts, SUBSEP)
            print "# " parts[1] ": missing exact strategy assignment: " parts[2]
            failed = 1
        }
    }
    exit failed
}
' "$source_file_h" "$source_file_c" "$interframe_tests_dir"/*.t || status=1

# These environment names are the supported registry surface. The behavioral
# policy owns their precedence and effects; ordinary tests assert those rules.
for env_name in \
    SIXEL_DITHER_STBN_SOURCE \
    SIXEL_DITHER_STBN_STRENGTH \
    SIXEL_DITHER_STBN_MOTION_ADAPT \
    SIXEL_DITHER_STBN_SCENE_CUT_RESET \
    SIXEL_DITHER_STBN_SCENE_DETECT \
    SIXEL_DITHER_STBN_ALPHA_GUARD \
    SIXEL_DITHER_STBN_PERCEPTUAL_WEIGHT \
    SIXEL_DITHER_STBN_FASTPATH \
    SIXEL_DITHER_STBN_DIFFUSION \
    SIXEL_DITHER_INTERFRAME_DIFFUSION
do
    grep -F -- "\"$env_name\"" "$registry_file" >/dev/null || {
        echo "# src/options-registry.c: missing environment name: $env_name"
        status=1
    }
done

for removed in SIXEL_DITHER_INTERFRAME_STRATEGY \
        SIXEL_DITHER_INTERFRAME_NOISE_STRENGTH; do
    if grep -qF -- "$removed" "$source_file_h" "$source_file_c" \
            "$registry_file"; then
        echo "# removed environment name: $removed"
        status=1
    fi
done

# Construct the retired alias so this checker does not reintroduce a use.
legacy_prefix=interframe
legacy_alias="$legacy_prefix-diffusion"
if grep -qF -- "$legacy_alias" "$interframe_tests_dir"/*.t \
        "$src_root"/tests/cli/options/matching/*.t; then
    echo "# legacy diffusion alias must not remain in tests"
    status=1
fi

test "$status" -eq 0 || {
    echo "not ok 1 - interframe strategy tokens stay synchronized"
    exit 1
}
echo "ok 1 - interframe strategy tokens stay synchronized"
exit 0

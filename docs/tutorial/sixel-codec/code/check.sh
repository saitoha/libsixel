#!/bin/sh
# SPDX-License-Identifier: MIT
#
# Check the SIXEL codec tutorial programs against each other.
#
#   ./check.sh [DIR]    DIR holds step1 ... step5 (default: this directory)
#
# 1. Every step round-trips the synthetic self-test images.
# 2. Encoders 2 to 5 produce byte-identical streams, for every thread count.
# 3. Decoders 2 to 5 produce identical images from every stream, including
#    the sample streams in images/ and a few unusual hand-written ones.
#    Step 1 is compared only where its simplifications do not matter.

set -u

here=$(cd "$(dirname "$0")" && pwd)
bin=$(cd "${1:-$here}" && pwd)
images="$here/../../../../images"
work=$(mktemp -d "${TMPDIR:-/tmp}/sixel-tutorial.XXXXXX") || exit 1
trap 'rm -rf "$work"' EXIT INT TERM
failures=0

fail() {
    echo "FAIL: $*"
    failures=$((failures + 1))
}

# same REF OUT DESCRIPTION COMMAND...: run COMMAND, which writes OUT, and
# require OUT to equal REF byte for byte.
same() {
    ref=$1 out=$2 what=$3
    shift 3
    if "$@" && cmp -s "$ref" "$out"; then
        return 0
    fi
    fail "$what"
}

for s in 1 2 3 4; do
    "$bin/step$s" selftest >/dev/null || fail "step$s selftest"
done
for t in 1 3 8; do
    TUTORIAL_THREADS=$t "$bin/step5" selftest >/dev/null ||
        fail "step5 selftest with $t threads"
done

# Inputs: the sample photograph and synthetic images of awkward sizes.
cp "$images/snake.ppm" "$work/snake.ppm"
for spec in "noise 257 49" "two 300 7" "sparse 1000 31" "gradient 64 64" \
            "flat 5 5"; do
    # shellcheck disable=SC2086 # split the spec into arguments
    set -- $spec
    "$bin/step1" synth "$1" "$2" "$3" "$work/$1.ppm" || fail "synth $1"
done

for img in "$work"/*.ppm; do
    name=$(basename "$img" .ppm)
    "$bin/step1" encode "$img" "$work/$name.1.six" || fail "step1 encode $name"
    "$bin/step2" encode "$img" "$work/$name.2.six" || fail "step2 encode $name"
    for s in 3 4; do
        same "$work/$name.2.six" "$work/$name.$s.six" \
            "step$s encoding of $name differs from step2" \
            "$bin/step$s" encode "$img" "$work/$name.$s.six"
    done
    for t in 1 2 5 16; do
        same "$work/$name.2.six" "$work/$name.5.six" \
            "step5 encoding of $name with $t threads differs from step2" \
            env TUTORIAL_THREADS="$t" "$bin/step5" encode "$img" "$work/$name.5.six"
    done
    # Every decoder reads both stream styles to the same image.
    for six in "$work/$name.1.six" "$work/$name.2.six"; do
        "$bin/step1" decode "$six" "$work/ref.ppm" || fail "step1 decode $six"
        for s in 2 3 4; do
            same "$work/ref.ppm" "$work/out.ppm" \
                "step$s decoding of $(basename "$six") differs" \
                "$bin/step$s" decode "$six" "$work/out.ppm"
        done
        for t in 1 3 16; do
            same "$work/ref.ppm" "$work/out.ppm" \
                "step5 decoding of $(basename "$six") with $t threads" \
                env TUTORIAL_THREADS="$t" "$bin/step5" decode "$six" "$work/out.ppm"
        done
    done
done

# Streams written by other encoders, and unusual ones: no raster
# attributes, paint beyond them, HLS colors, redefinition after paint,
# repeats crossing the raster edge, empty bands and a missing terminator.
cp "$images/map8.six" "$images/map64.six" "$work/"
# shellcheck disable=SC1003 # '\\' is the backslash of ST, not an escape
{
    printf '\033Pq#1;2;100;0;0#1~~~$#2;2;0;100;0!5N-#1!3@\033\\' >"$work/noraster.six"
    printf '\033Pq"1;1;4;4#0;1;120;50;100!10~-?-#0F\033\\' >"$work/beyond.six"
    printf '\033Pq"1;1;8;6#3;2;10;20;30#3!8~#3;2;90;80;70$!4o\033\\' >"$work/redefine.six"
    printf '\033Pq"1;1;2;2--#1;2;50;50;50!0~!2A\033\\' >"$work/emptybands.six"
    printf '\033Pq#1;2;0;0;100~~-~' >"$work/unterminated.six"
}
for six in "$work"/map8.six "$work"/map64.six "$work"/noraster.six \
           "$work"/beyond.six "$work"/redefine.six "$work"/emptybands.six \
           "$work"/unterminated.six; do
    "$bin/step2" decode "$six" "$work/ref.ppm" ||
        fail "step2 decode $(basename "$six")"
    for s in 3 4; do
        same "$work/ref.ppm" "$work/out.ppm" \
            "step$s decoding of $(basename "$six") differs from step2" \
            "$bin/step$s" decode "$six" "$work/out.ppm"
    done
    for t in 1 2 4; do
        same "$work/ref.ppm" "$work/out.ppm" \
            "step5 decoding of $(basename "$six") with $t threads" \
            env TUTORIAL_THREADS="$t" "$bin/step5" decode "$six" "$work/out.ppm"
    done
done

if [ "$failures" -ne 0 ]; then
    echo "$failures check(s) failed"
    exit 1
fi
echo "all checks passed"

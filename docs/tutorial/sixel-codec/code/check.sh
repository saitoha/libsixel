#!/bin/sh
# SPDX-License-Identifier: MIT
#
# Check the SIXEL codec tutorial programs against each other.
#
#   ./check.sh [DIR]    DIR holds the chapter programs (default: here)
#
# 1. Every program round-trips the synthetic self-test images.
# 2. From packing on, every encoder writes the same bytes as packing, for
#    every thread count. The nodes encoder writes other bytes, which decode
#    to the same pixels and are never longer than the first codec's.
# 3. From full-decoder on, every decoder reads every stream to the same
#    image, including the sample streams in images/ and a few unusual
#    hand-written ones. The first codec's decoder, which first-codec, nodes
#    and packing share, is compared only where its simplifications do not
#    matter.
# 4. Overpaint writes a different stream that every decoder reads to the
#    same pixels, and that stream is never longer than packing's.

set -u

here=$(cd "$(dirname "$0")" && pwd)
bin=$(cd "${1:-$here}" && pwd)
images="$here/../../../../images"
work=$(mktemp -d "${TMPDIR:-/tmp}/sixel-tutorial.XXXXXX") || exit 1
trap 'rm -rf "$work"' EXIT INT TERM
failures=0

# Encoders that must write packing's bytes, and decoders that must read like
# full-decoder. The two threaded programs are checked separately.
same_encoders="full-decoder fewer-questions masks next-node column-words
    swar first-colors occupancy word-decoder vector-lanes transpose"
full_decoders="fewer-questions masks next-node column-words swar
    first-colors occupancy word-decoder vector-lanes transpose parallel-bands"
serial="first-codec nodes packing full-decoder $same_encoders overpaint"
threaded="parallel-bands parallel-decoder"

fail() {
    echo "FAIL: $*"
    failures=$((failures + 1))
}

# same REF OUT DESCRIPTION COMMAND...: run COMMAND with its standard
# output in OUT, and require OUT to equal REF byte for byte.
same() {
    ref=$1 out=$2 what=$3
    shift 3
    if "$@" >"$out" && cmp -s "$ref" "$out"; then
        return 0
    fi
    fail "$what"
}

# shellcheck disable=SC2086 # the lists are split on purpose
for s in $(echo $serial | tr ' ' '\n' | sort -u); do
    "$bin/$s" selftest >/dev/null || fail "$s selftest"
done
for s in $threaded; do
    for t in 1 3 8; do
        TUTORIAL_THREADS=$t "$bin/$s" selftest >/dev/null ||
            fail "$s selftest with $t threads"
    done
done

# Inputs: the sample photograph and synthetic images of awkward sizes.
cp "$images/snake.ppm" "$work/snake.ppm"
for spec in "noise 257 49" "two 300 7" "sparse 1000 31" "gradient 64 64" \
            "flat 5 5"; do
    # shellcheck disable=SC2086 # split the spec into arguments
    set -- $spec
    "$bin/first-codec" synth "$1" "$2" "$3" >"$work/$1.ppm" ||
        fail "synth $1"
done

for img in "$work"/*.ppm; do
    name=$(basename "$img" .ppm)
    "$bin/first-codec" encode "$img" >"$work/$name.first.six" ||
        fail "first-codec encode $name"
    "$bin/nodes" encode "$img" >"$work/$name.nodes.six" ||
        fail "nodes encode $name"
    if [ "$(wc -c <"$work/$name.nodes.six")" -gt \
         "$(wc -c <"$work/$name.first.six")" ]; then
        fail "nodes encoding of $name is longer than first-codec's"
    fi
    "$bin/packing" encode "$img" >"$work/$name.packing.six" ||
        fail "packing encode $name"
    for s in $same_encoders; do
        same "$work/$name.packing.six" "$work/$name.out.six" \
            "$s encoding of $name differs from packing" \
            "$bin/$s" encode "$img"
    done
    for s in $threaded; do
        for t in 1 2 5 16; do
            same "$work/$name.packing.six" "$work/$name.out.six" \
                "$s encoding of $name with $t threads differs from packing" \
                env TUTORIAL_THREADS="$t" "$bin/$s" encode "$img"
        done
    done
    "$bin/overpaint" encode "$img" >"$work/$name.overpaint.six" ||
        fail "overpaint encode $name"
    if [ "$(wc -c <"$work/$name.overpaint.six")" -gt \
         "$(wc -c <"$work/$name.packing.six")" ]; then
        fail "overpaint encoding of $name is longer than packing's"
    fi
    # Every decoder reads every stream style to the same image.
    for six in "$work/$name.first.six" "$work/$name.nodes.six" \
               "$work/$name.packing.six" "$work/$name.overpaint.six"; do
        "$bin/first-codec" decode "$six" >"$work/ref.ppm" ||
            fail "first-codec decode $six"
        for s in full-decoder $full_decoders overpaint; do
            same "$work/ref.ppm" "$work/out.ppm" \
                "$s decoding of $(basename "$six") differs" \
                "$bin/$s" decode "$six"
        done
        for t in 1 3 16; do
            same "$work/ref.ppm" "$work/out.ppm" \
                "parallel-decoder decoding of $(basename "$six"), $t threads" \
                env TUTORIAL_THREADS="$t" \
                "$bin/parallel-decoder" decode "$six"
        done
    done
done

# Gray input: a gray level is a palette index, so a text P2 file and the
# same picture as binary P5 encode identically, and the P2 heart of the
# tutorial's lesson 0 produces the lesson's body.
printf 'P2\n7 6\n1\n0 1 1 0 1 1 0\n1 1 1 1 1 1 1\n1 1 1 1 1 1 1\n0 1 1 1 1 1 0\n0 0 1 1 1 0 0\n0 0 0 1 0 0 0\n' >"$work/heart.pgm"
{
    printf 'P5 7 6 1\n'
    printf '\000\001\001\000\001\001\000\001\001\001\001\001\001\001'
    printf '\001\001\001\001\001\001\001\000\001\001\001\001\001\000'
    printf '\000\000\001\001\001\000\000\000\000\000\001\000\000\000'
} >"$work/heart5.pgm"
for s in first-codec nodes packing $same_encoders $threaded; do
    "$bin/$s" encode "$work/heart.pgm" >"$work/heart.$s.six" ||
        fail "$s encode heart.pgm"
    same "$work/heart.$s.six" "$work/heart5.$s.six" \
        "$s encodes P2 and P5 differently" \
        "$bin/$s" encode "$work/heart5.pgm"
    grep -q '#1EN^}^NE' "$work/heart.$s.six" ||
        fail "$s heart body differs from lesson 0"
    same "$work/heart.$s.six" "$work/heart.stdin.six" \
        "$s encodes standard input differently" \
        "$bin/$s" encode <"$work/heart.pgm"
done

# The tallest pictures: the last band extends past the size limit, and every
# program must read its own stream.
for h in 32767 32768; do
    printf 'P5 1 %d 1\n' "$h" >"$work/tall.pgm"
    head -c "$h" /dev/zero >>"$work/tall.pgm"
    for s in first-codec nodes packing $same_encoders $threaded overpaint; do
        if "$bin/$s" encode "$work/tall.pgm" >"$work/tall.six" &&
                "$bin/$s" decode "$work/tall.six" >"$work/tall.ppm" &&
                [ "$(sed -n 2p "$work/tall.ppm")" = "1 $h" ]; then
            :
        else
            fail "$s cannot read its own 1x$h stream"
        fi
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
    printf 'quit\033Pq"1;1;3;6#1;2;100;0;0#1~~~\033\\' >"$work/preamble.six"
}
for six in "$work"/map8.six "$work"/map64.six "$work"/noraster.six \
           "$work"/beyond.six "$work"/redefine.six "$work"/emptybands.six \
           "$work"/unterminated.six "$work"/preamble.six; do
    "$bin/full-decoder" decode "$six" >"$work/ref.ppm" ||
        fail "full-decoder decode $(basename "$six")"
    for s in $full_decoders overpaint; do
        same "$work/ref.ppm" "$work/out.ppm" \
            "$s decoding of $(basename "$six") differs" \
            "$bin/$s" decode "$six"
    done
    for t in 1 2 4; do
        same "$work/ref.ppm" "$work/out.ppm" \
            "parallel-decoder decoding of $(basename "$six"), $t threads" \
            env TUTORIAL_THREADS="$t" "$bin/parallel-decoder" decode "$six"
    done
done

if [ "$failures" -ne 0 ]; then
    echo "$failures check(s) failed"
    exit 1
fi
echo "all checks passed"

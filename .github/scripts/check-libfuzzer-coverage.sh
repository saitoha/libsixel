#!/bin/sh
# Replay fixed inputs to measure symbolized edges inside the target code.
set -eu

buildtool=$1
harness=$2
target=$3
log=$4
shift 4
table=.github/scripts/libfuzzer-coverage.tsv

case "$buildtool" in
    autotools) baseline_column=4 ;;
    meson) baseline_column=5 ;;
    *) echo "Unknown build system: $buildtool" >&2; exit 2 ;;
esac

if [ "$#" -eq 0 ]; then
    case "$target" in
        struct-*)
            # Replay the same small inputs across the structured harnesses.
            set -- tests/data/inputs/snake_64.bmp \
                tests/data/inputs/snake_64.gif tests/data/inputs/snake_64.png \
                tests/data/inputs/snake_64.ppm \
                tests/data/inputs/formats/stbi_gradient16x16.hdr \
                tests/data/inputs/formats/stbi_minimal.pic \
                tests/data/inputs/formats/stbi_minimal_indexed16_raw.psd
            ;;
        *)
            seed=$(awk -v target="$target" '$1 == target { print $6 }' "$table")
            test -n "$seed"
            set -- "tests/data/inputs/$seed"
            ;;
    esac
fi

# Use a corpus directory so libFuzzer reports observed edges on exit.
corpus=$(mktemp -d)
trap 'rm -rf "$corpus"' EXIT
trap 'exit 1' HUP INT TERM
for seed in "$@"; do
    cp "$seed" "$corpus/"
done

export ASAN_SYMBOLIZER_PATH
ASAN_SYMBOLIZER_PATH=$(command -v llvm-symbolizer-21)
"$harness" -runs=0 -seed=1 -timeout=5 -print_coverage=1 \
    -artifact_prefix="${log%/*}/" "$corpus" > "$log" 2>&1 || {
    tail -100 "$log"
    exit 1
}

# Check the decoder (or library API) functions and require a decoded frame
# from each format probe. Leave 20% headroom for compiler and source changes.
awk -v target="$target" -v column="$baseline_column" '
FNR == NR {
    if ($1 == target) {
        function_pattern = $2
        file_pattern = "/src/" $3 ":[0-9]+$"
        baseline = $column
    }
    next
}
/^#[0-9]+[[:space:]]+DONE[[:space:]]+cov:/ { completed = 1 }
/^COVERAGE:$/ { reported = 1 }
$1 == "COVERED_FUNC:" && $6 == "fuzz_frame_callback" &&
    $7 ~ /\/fuzz\/fuzz-loader-builtin-libfuzzer\.c:/ { frame = 1 }
$1 == "COVERED_FUNC:" && baseline && $6 ~ function_pattern {
    source = $7
    gsub(/\/\.\//, "/", source)
    if (source !~ file_pattern) next
    split($5, counts, "/")
    edges += counts[1]
    print
}
END {
    minimum = baseline - int(baseline / 5)
    printf "%s: %d covered edges in target functions (baseline %d, minimum %d)\n",
           target, edges, baseline, minimum
    if (!baseline || !completed || !reported || edges < minimum) exit 1
    if (target !~ /^(struct-|encoder-|helper-)/ && !frame) {
        print target ": the decoder did not emit a frame"
        exit 1
    }
}
' "$table" "$log" || {
    echo "::error::Fixed corpus coverage failed for $target. See $log." >&2
    exit 1
}

#!/bin/bash
# Replay the checked-in raw ICC corpus on push and during failure triage.
set -euxo pipefail

buildtool=$1
export TOP_SRCDIR="$PWD"
export PATH="$TOP_SRCDIR/.local/bin:$PWD/build-aux:$PWD/bin:$PATH"
harness_name=fuzz-cms-builtin-libfuzzer
artifact_dir="$PWD/.tmp/fuzz-artifacts-cms-builtin"
mkdir -p "$artifact_dir"

sh .github/scripts/build-linux-libfuzzer.sh "$buildtool" "$harness_name"
harness="$PWD/fuzz/$harness_name"
if test "$buildtool" = meson; then
    harness="$PWD/builddir/fuzz/$harness_name"
fi

# Corpus initialization runs every seed even when mutations are disabled.
"$harness" -runs=0 -seed=1 -timeout=5 -print_final_stats=1 \
    -artifact_prefix="$artifact_dir/" "$PWD/fuzz/corpus/cms-builtin" 2>&1 \
    | tee "$artifact_dir/replay.log"
grep -Eq 'DONE[[:space:]]+cov:' "$artifact_dir/replay.log"

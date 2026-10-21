#!/bin/sh
# Collect source-owned references once per suite, retaining each rule verdict.
set -eu

src_root=$1
output_root=$2
scope=${3:-all}

# Each awk batch appends to the same bounded set of outputs. Awk retains open
# streams within a batch; initializing here prevents later batches truncating
# earlier results. Standalone checks use the same collector with their scope.
: > "$output_root/source-contracts"
: > "$output_root/policy-links"
: > "$output_root/policy-errors"
: > "$output_root/test-plan-links"
: > "$output_root/test-plan-errors"

find "$src_root/tests" -type f \
    \( -name '*.t' -o -name '*.c' -o -name '*.sh' -o \
       -name '*.py' -o -name '*.rb' -o -name '*.pl' -o \
       -name '*.php' -o -name '*.ps1' \) \
    -exec awk -v src_root="$src_root/" -v output_root="$output_root" \
        -v scope="$scope" -f "${0%/*}/../awk/source-references.awk" {} +

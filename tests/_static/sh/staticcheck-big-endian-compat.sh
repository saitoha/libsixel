#!/bin/sh
# Verify that word-sized encoder comparisons preserve memory byte order.
# Policy: docs/misc/platforms/big-endian.md
# Coverage: BE-01 BE-02

set -eu

src_root=$1
source_file="$src_root/src/encoder-core-encode.c"
workflow="$src_root/.github/workflows/experimental.yml"
failed=0

fail()
{
    echo "# $*" >&2
    failed=1
}

require_fixed()
{
    pattern=$1

    grep -F -- "$pattern" "$source_file" >/dev/null 2>&1 ||
        fail "src/encoder-core-encode.c is missing: $pattern"
}

echo "1..1"

require_fixed 'memcpy(&block,'
require_fixed 'map + index + run_length,'
require_fixed 'block ^= pattern;'
require_fixed \
    'block_bytes = (unsigned char const *)(void const *)&block;'
require_fixed 'if (block_bytes[byte_index] != 0U) {'

if grep -F 'block >>= 8;' "$source_file" >/dev/null 2>&1; then
    fail "word mismatch scanning must not assume least-significant-byte order"
fi

s390_jobs=$(awk 'index($0, "-s390x-gcc") { count++ }
                 END { print count + 0 }' "$workflow")
test "$s390_jobs" -eq 2 ||
    fail "experimental CI must retain two Linux/s390x build-system jobs"
grep -F '  sparc64-netbsd:' "$workflow" >/dev/null 2>&1 ||
    fail "experimental CI is missing the NetBSD/SPARC64 job family"
grep -F 'buildtool: [autotools, meson]' "$workflow" >/dev/null 2>&1 ||
    fail "NetBSD/SPARC64 must retain both build systems"
grep -F 'shard: [1, 2, 3, 4, 5, 6, 7, 8]' \
    "$workflow" >/dev/null 2>&1 ||
    fail "NetBSD/SPARC64 must retain all eight shards"
grep -F 'build-aux/read-check-test-list.sh' \
    "$workflow" >/dev/null 2>&1 ||
    fail "SPARC64 Autotools shards must use the registered test inventory"
grep -F '((NR - 1) % 8) + 1 == shard' "$workflow" >/dev/null 2>&1 ||
    fail "SPARC64 Autotools shards must partition the complete inventory"
grep -F -- "--slice '\${{ matrix.shard }}/8'" \
    "$workflow" >/dev/null 2>&1 ||
    fail "SPARC64 Meson shards must partition the complete suite"

test "$failed" -eq 0 || {
    echo "not ok 1 - big-endian source and CI contracts are preserved"
    exit 1
}

echo "ok 1 - big-endian source and CI contracts are preserved"
exit 0

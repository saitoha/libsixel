#!/bin/sh
# Every runtime case belongs to exactly one macOS Autotools TSan shard.
# Policy: docs/ci/design.md
# Policy: docs/build/test-harness.md
# Policy: docs/testing/staticcheck.md
set -eux
echo '1..1'
set -v
awk '
/^  tsan:/ { active = 1; next }
active && /^  [^ ]/ { active = 0 }
active && /timeout-minutes: 360/ { bounded = 1 }
active && /label: Autotools-macos-aarch64-TSan-/ { labels++ }
active && /test_shard:/ { shards[$2]++ }
/SIXEL_TEST_SHARD=.*fromJSON\(inputs.matrix_json\).test_shard/ { routed = 1 }
/read-check-test-list.sh.*include newline.*SIXEL_TEST_SHARD/ { selected = 1 }
END { exit !(bounded && labels == 3 && shards["1/3"] == 1 &&
             shards["2/3"] == 1 && shards["3/3"] == 1 && routed && selected) }
' "$1/.github/workflows/ci.yml" "$1/.github/actions/ci-steps/action.yml" \
    "$1/tests/Makefile.am" || {
    echo 'not ok 1 - macOS TSan routes all three bounded shards to discovery'
    exit 0
}
{
    echo __SHARD_0
    sh "$1/build-aux/read-check-test-list.sh" tests "$1/tests" include newline ||
        echo __FAILED
    for shard in 1 2 3; do
        echo "__SHARD_$shard"
        sh "$1/build-aux/read-check-test-list.sh" tests "$1/tests" include newline "$shard/3" ||
            echo __FAILED
    done
} | awk '
/^__SHARD_/ { shard = substr($0, 9) + 0; next }
/^__FAILED$/ { failed = 1; next }
shard == 0 { expected[$0] = count++ % 3 + 1; next }
{ if (!($0 in expected) || expected[$0] != shard || seen[$0]++) failed = 1; used++ }
END { exit failed || count == 0 || count != used }
' || {
    echo 'not ok 1 - TSan shards form a complete disjoint test partition'
    exit 0
}
for invalid in 0/3 4/3 1/0 bad; do
    sh "$1/build-aux/read-check-test-list.sh" tests "$1/tests" include newline "$invalid" >/dev/null && {
        echo 'not ok 1 - invalid shard selections must fail'
        exit 0
    }
done
echo 'ok 1 - TSan partitions every runtime test exactly once'
exit 0

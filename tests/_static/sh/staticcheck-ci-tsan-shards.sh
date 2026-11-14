#!/bin/sh
# Check macOS TSan shard wiring and Autotools test membership.
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
active && /fail-fast: false/ { independent = 1 }
active && /- label:/ { label = $3 }
active && label ~ /^(Autotools|Meson)-macos-aarch64-TSan/ &&
    /- label:/ { labels[label]++ }
active && /test_shard:/ { shards[label, $2]++ }
/SIXEL_TEST_SHARD=.*fromJSON\(inputs.matrix_json\).test_shard/ { routed = 1 }
/SIXEL_TEST_SHARD:.*fromJSON\(inputs.matrix_json\).test_shard/ { meson_env = 1 }
/meson test -C builddir --slice "\$SIXEL_TEST_SHARD"/ { meson_slice = 1 }
/read-check-test-list.sh.*include newline.*SIXEL_TEST_SHARD/ { selected = 1 }
END {
    for (i = 1; i <= 3; i++) {
        label = "Autotools-macos-aarch64-TSan-" i "-of-3"
        if (labels[label] != 1 || shards[label, i "/3"] != 1) exit 1
    }
    for (i = 1; i <= 6; i++) {
        label = "Meson-macos-aarch64-TSan-" i "-of-6"
        if (labels[label] != 1 || shards[label, i "/6"] != 1) exit 1
    }
    for (label in labels) label_count++
    for (shard in shards) shard_count++
    exit !(bounded && independent && label_count == 9 && shard_count == 9 &&
           routed && selected && meson_env && meson_slice)
}
' "$1/.github/workflows/ci.yml" "$1/.github/actions/ci-steps/action.yml" \
    "$1/tests/Makefile.am" || {
    echo 'not ok 1 - macOS TSan routes every bounded shard to its test runner'
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
echo 'ok 1 - macOS TSan configures complete test partitions'
exit 0

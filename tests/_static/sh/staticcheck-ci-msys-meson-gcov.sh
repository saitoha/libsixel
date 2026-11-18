#!/bin/sh
# Protect shared MSYS GCC profiles from concurrent Meson test processes.
set -eux
echo '1..1'
set -v

awk '
function check_shard(    count, fields, shard, total) {
    if (label == "") return
    count = split(label, fields, "-")
    shard = fields[1] + 0
    total = fields[3] + 0
    if (count != 3 || fields[2] != "of" || shard < 1 ||
            shard > total || total < 2 || label != shard "-of-" total ||
            (shard_total && shard_total != total) || seen[shard]++ ||
            selector != shard "/" total || test_command != "test --num-processes 1") {
        print "# MSYS GCC coverage requires unique serial shards: " label
        failed = 1
    }
    shard_total = total
    ++shards
    label = ""
}
FNR == 1 { check_shard(); msys_step = 0 }
FILENAME == ARGV[1] && /^          - label: / {
    check_shard()
    if ($3 ~ /^Meson-msys2-msys-gcc-coverage/) {
        label = $3
        sub(/^Meson-msys2-msys-gcc-coverage-/, "", label)
        selector = test_command = ""
    }
    next
}
FILENAME == ARGV[1] && label != "" {
    if (/^            test_shard: /) selector = $2
    if (/^            check_target: /) {
        test_command = $0
        sub(/^            check_target: /, "", test_command)
    }
}
FILENAME == ARGV[2] && /^    - name: / {
    msys_step = ($0 == "    - name: Build and test (Meson, msys2)")
}
msys_step && /^[[:space:]]*meson .*check_target.*-C builddir$/ { ++routed }
END {
    check_shard()
    if (shards != shard_total || shards < 2 || routed != 1) {
        print "# MSYS GCC coverage needs a complete shard set and check_target routing"
        failed = 1
    }
    exit failed
}
' "$1/.github/workflows/ci.yml" "$1/.github/actions/ci-steps/action.yml" || {
    echo 'not ok 1 - MSYS Meson GCC coverage shards run tests serially'
    exit 0
}
echo 'ok 1 - MSYS Meson GCC coverage shards run tests serially'
exit 0

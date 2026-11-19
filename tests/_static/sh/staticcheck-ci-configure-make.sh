#!/bin/sh
# Keep the configured tool environment across CI's nested configure calls.
# Policy: docs/ci/design.md
# Policy: docs/testing/staticcheck.md
set -eux
echo '1..1'
set -v
src_root=$1
awk '
function check(text,    n, lines, i, bound) {
    n = split(text, lines, "\n")
    for (i = 1; i <= n; ++i) {
        if (lines[i] ~ /export MAKE=.*fromJSON\(inputs.matrix_json\)[.]make/)
            bound = 1
        if (lines[i] ~ /\/configure([[:space:]]|$)/) return bound
    }
    return 0
}
function check_msys(text,    n, lines, i, command) {
    n = split(text, lines, "\n")
    for (i = 1; i <= n; ++i) {
        command = command lines[i]
        if (sub(/\\[[:space:]]*$/, "", command)) continue
        if (command ~ /[.]setup_prefix.*[.]make.*SIXEL_TEST_SHARD=.*[.]check_target/)
            return 1
        command = ""
    }
    return 0
}
BEGIN {
    bind = "export MAKE=fromJSON(inputs.matrix_json).make"
    if (!check(bind "\n./configure") || check("./configure") ||
            check("./configure\n" bind)) exit 1
}
/^    - name:/ {
    active = /Build and test \(Autotools, FreeBSD\/OpenBSD\/NetBSD\/Haiku\)/
    if (active) ++steps
    msys = /Build and test \(Autotools, msys2\)/
    if (msys) ++msys_steps
}
active { source = source $0 "\n" }
msys { msys_source = msys_source $0 "\n" }
END {
    if (steps != 1 || !check(source)) {
        print "VM configure must inherit the selected make"
        exit 1
    }
    if (msys_steps != 1 || !check_msys(msys_source)) {
        print "MSYS2 make must carry setup variables into distcheck"
        exit 1
    }
}
' "$src_root/.github/actions/ci-steps/action.yml" || {
    echo 'not ok 1 - CI nested configure inherits the selected tools'
    exit 0
}
echo 'ok 1 - CI nested configure inherits the selected tools'
exit 0

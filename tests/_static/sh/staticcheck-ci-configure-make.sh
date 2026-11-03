#!/bin/sh
# The VM's selected make must reach configure's dependency bootstrap.
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
BEGIN {
    bind = "export MAKE=fromJSON(inputs.matrix_json).make"
    if (!check(bind "\n./configure") || check("./configure") ||
            check("./configure\n" bind)) exit 1
}
/^    - name:/ {
    active = /Build and test \(Autotools, FreeBSD\/OpenBSD\/NetBSD\/Haiku\)/
    if (active) ++steps
}
active { source = source $0 "\n" }
END { if (steps != 1 || !check(source)) exit 1 }
' "$src_root/.github/actions/ci-steps/action.yml" || {
    echo 'not ok 1 - VM configure inherits the selected make'
    exit 0
}
echo 'ok 1 - VM configure inherits the selected make'
exit 0

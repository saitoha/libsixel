#!/bin/sh
# Nightly Windows checkouts must preserve shell source line endings.
# Policy: docs/ci/design.md
# Policy: docs/testing/staticcheck.md
set -eux
echo '1..1'
set -v
src_root=$1
awk '
function check(text,    n, lines, i, configured) {
    n = split(text, lines, "\n")
    for (i = 1; i <= n; ++i) {
        if (lines[i] ~ /run: git config --global core[.]autocrlf false[[:space:]]*$/)
            configured = 1
        if (lines[i] ~ /uses: actions\/checkout@/) return configured
    }
    return 0
}
BEGIN {
    setup = "run: git config --global core.autocrlf false"
    checkout = "uses: actions/checkout@v4"
    if (!check(setup "\n" checkout) || check(checkout) ||
            check(checkout "\n" setup) ||
            check("run: git config --global core.autocrlf true\n" checkout))
        exit 1
}
{ source = source $0 "\n" }
END { if (!check(source)) exit 1 }
' "$src_root/.github/workflows/nightly.yml" || {
    echo 'not ok 1 - Nightly checkout preserves shell line endings'
    exit 0
}
echo 'ok 1 - Nightly checkout preserves shell line endings'
exit 0

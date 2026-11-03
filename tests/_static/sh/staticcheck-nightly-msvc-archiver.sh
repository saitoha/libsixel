#!/bin/sh
# Nightly must exercise configure's MSVC archiver selection.
# Policy: docs/ci/design.md
# Policy: docs/testing/staticcheck.md
set -eux
echo '1..1'
set -v
src_root=$1
awk '
function check(text,    n, lines, i, configure_seen) {
    n = split(text, lines, "\n")
    for (i = 1; i <= n; ++i) {
        if (lines[i] ~ /^[[:space:]]*#/) continue
        if (lines[i] ~ /(^|[[:space:]])AR(FLAGS)?=/) return 0
        if (lines[i] ~ /[.]\/configure CC=cl([[:space:]]|$)/)
            configure_seen = 1
    }
    return configure_seen
}
BEGIN {
    configure = "./configure CC=cl --host=x86_64-pc-windows-msvc"
    if (!check(configure) || check("export AR=stale\n" configure) ||
            check(configure " AR=wrapper") ||
            check(configure "\n ARFLAGS=cr") || check("make")) exit 1
}
/^    - name:/ {
    active = /Configure and build \(Windows, MSVC\)/
    if (active) ++steps
}
active { source = source $0 "\n" }
END { if (steps != 1 || !check(source)) exit 1 }
' "$src_root/.github/workflows/nightly.yml" || {
    echo 'not ok 1 - Nightly uses configure MSVC archiver defaults'
    exit 0
}
echo 'ok 1 - Nightly uses configure MSVC archiver defaults'
exit 0

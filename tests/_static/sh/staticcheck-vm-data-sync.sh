#!/bin/sh
# Policy: docs/ci/design.md
# Policy: docs/testing/staticcheck.md
# VM disks must stay outside the action source-transfer tree ($HOME/work).
set -eux
echo '1..1'
set -v
awk '
function safe(path) {
    # Host HOME cache paths avoid runner.temp and workspace storage below work.
    return path ~ /^\$HOME\/\.cache\/[-a-zA-Z0-9_.\/]+$/ &&
        path !~ /\/(\.|\.\.)(\/|$)/
}
/^[ \t]*data-dir:/ {
    path = $0
    sub(/^[ \t]*data-dir:[ \t]*/, "", path)
    sub(/[ \t]*$/, "", path)
    count++
    if (!safe(path)) {
        print FILENAME ":" FNR ": VM data must use a host HOME cache path"
        failed = 1
    }
}
END {
    # Check the same path predicate against transfers of runner-owned storage.
    if (!safe("$HOME/.cache/libsixel-sparc-vm-1") ||
        safe("${{ runner.temp }}/sparc-vm-1") ||
        safe("${{ github.workspace }}/sparc-vm-1") ||
        safe("$HOME/work/_temp/sparc-vm-1") ||
        safe("/home/runner/work/_temp/sparc-vm-1") ||
        safe("$HOME/.cache/../work/sparc-vm-1")) failed = 1
    if (!count) {
        print "VM data directory coverage is empty"
        failed = 1
    }
    exit failed
}' "$1"/.github/workflows/*.yml >&2 || {
    echo 'not ok 1 - VM disk data stays outside source synchronization'
    exit 0
}
echo 'ok 1 - VM disk data stays outside source synchronization'

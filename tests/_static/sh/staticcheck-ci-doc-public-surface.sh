#!/bin/sh
# Keep CI documentation limited to repository-visible sources and contracts.
# Policy: docs/ci/design.md

set -eu

src_root=$1
matches=$(find "$src_root/docs/ci" -type f -name '*.md' -exec awk \
    -v forbidden='libsixel-ci|`/?srv/|jobs[.]tsv|runner-profiles[.]tsv|workers[.]tsv|resource-classes[.]tsv|job-definition/' '
    $0 ~ forbidden {
        print FILENAME ":" FNR ":" $0
    }
' {} +)

echo "1..1"

test -z "$matches" || {
    echo "not ok 1 - CI docs expose private repository paths"
    printf '%s\n' "$matches" | sed 's/^/# /'
    exit 1
}

echo "ok 1 - CI docs describe only repository-visible sources and contracts"
exit 0

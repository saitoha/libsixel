#!/bin/sh
# All virtual-machine jobs share one repository-wide serial queue.
# Policy: docs/ci/design.md
# Policy: docs/testing/staticcheck.md
set -eux
echo '1..1'
set -v
awk -f "$1/tests/_static/awk/ci-vm-concurrency.awk" \
    "$1/.github/workflows/ci.yml" \
    "$1/.github/workflows/experimental.yml" || {
    echo 'not ok 1 - VM jobs must share a non-cancelling serial queue'
    exit 0
}
echo 'ok 1 - VM jobs serialize across normal and experimental workflows'
exit 0

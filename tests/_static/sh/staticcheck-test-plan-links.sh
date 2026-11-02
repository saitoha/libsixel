#!/bin/sh
# Verify complete reciprocal links between test plans and covered tests.
# Policy: docs/AGENTS.md
# Policy: docs/testing/guide.md
# Policy: docs/testing/staticcheck.md
set -eux
echo '1..1'
set -v
src_root=$1
sh "${0%/*}/check-test-plan-inventory.sh" "$src_root" || {
    echo 'not ok 1 - test-plan inventories and covered tests are reciprocal'
    exit 0
}
LC_ALL=C awk -v root="$src_root" \
    -f "$src_root/tests/_static/awk/builtin-loader-coverage.awk" \
    "$src_root/docs/testing/builtin-loader-coverage.md" || {
    echo 'not ok 1 - test-plan inventories and covered tests are reciprocal'
    exit 0
}
echo 'ok 1 - test-plan inventories and covered tests are reciprocal'
exit 0

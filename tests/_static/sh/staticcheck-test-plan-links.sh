#!/bin/sh
# Verify complete reciprocal links between test plans and covered tests.
# Policy: docs/AGENTS.md
# Policy: docs/testing/guide.md
# Policy: docs/testing/staticcheck.md
set -eu
echo '1..1'
"${PYTHON_STATICCHECK:-python3}" "${0%/*}/../python/test_plan_links.py" "$1" || {
    echo 'not ok 1 - test-plan inventories and covered tests are reciprocal'
    exit 1
}
echo 'ok 1 - test-plan inventories and covered tests are reciprocal'

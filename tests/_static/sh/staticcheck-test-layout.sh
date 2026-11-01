#!/bin/sh
# Policy: docs/testing/guide.md
# Check all source cases, independently of enabled platform features.
set -eu
echo '1..1'
"${PYTHON_STATICCHECK:-python3}" "${0%/*}/../python/test_layout.py" \
    "$1/tests" || {
    echo 'not ok 1 - test categories obey depth, capacity and numbering rules'
    exit 1
}
echo 'ok 1 - test categories obey depth, capacity and numbering rules'

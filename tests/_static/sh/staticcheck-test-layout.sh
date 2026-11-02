#!/bin/sh
# Policy: docs/testing/guide.md
# Check all source cases, independently of enabled platform features.
set -eux
echo '1..1'
set -v
sh "$1/tests/_static/sh/check-test-layout.sh" "$1/tests" >&2 || {
    echo 'not ok 1 - test categories obey depth, capacity and numbering rules'
    exit 0
}
echo 'ok 1 - test categories obey depth, capacity and numbering rules'
exit 0

#!/bin/sh
# Pattern predicates use case; exceptions require explicit user approval.
# Policy: docs/testing/guide.md
set -eu

echo '1..1'
src_root=$1
python_bin=${2:-python3}

# This is a mandatory source policy. Missing audit tooling is not a pass/skip.
test -n "$python_bin" || {
    echo 'not ok 1 - shell pattern policy requires Python 3'
    exit 0
}
"$python_bin" "$src_root/tests/_static/python/test_pattern_match.py" \
    "$src_root" || {
    echo 'not ok 1 - shell pattern predicates use case or approved exceptions'
    exit 0
}
echo 'ok 1 - shell pattern predicates use case or approved exceptions'
exit 0

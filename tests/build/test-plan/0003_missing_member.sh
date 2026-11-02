#!/bin/sh
set -eux
# shellcheck source=tests/build/test-plan/fixture.sh
. "${0%/*}/fixture.sh"
rm "$work/$member"
reject 'missing scoped test'

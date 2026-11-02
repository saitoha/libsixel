#!/bin/sh
# Policy: docs/testing/guide.md
set -eux
# shellcheck source=tests/build/layout/fixture.sh
. "${0%/*}/fixture.sh"
add _support/target
mkdir -p "$work/owner/behavior"
ln -s "$work/_support/target" "$work/owner/behavior/0001_link.t"
reject 'regular source file'

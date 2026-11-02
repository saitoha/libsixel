#!/bin/sh
# Policy: docs/testing/guide.md
set -eux
# shellcheck source=tests/build/layout/fixture.sh
. "${0%/*}/fixture.sh"
cases 201
reject '201 cases exceed 200'

#!/bin/sh
# Keep standalone Windows probes out of aggregate runner discovery.
set -eu
SIXEL_SCAN_NAMES='*.c'
SIXEL_SCAN_EXCLUDES=
SIXEL_SCAN_PATHS='*/extension/wic-installed/*.c'
export SIXEL_SCAN_NAMES SIXEL_SCAN_EXCLUDES SIXEL_SCAN_PATHS
result=$(printf '%s\n' 'C:\repo\tests\extension\wic-installed\probe.c' 'C:\repo\tests\unit\probe.c' |
    awk -v max_depth=-1 -v absolute='C:/repo/tests' -v requested='C:/repo/tests' \
        -f "$1/tests/_static/awk/source-inventory-select.awk")
test "$result" = 'C:/repo/tests/unit/probe.c'

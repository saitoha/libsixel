#!/bin/sh
# DIN99d normalization maps and clips signed chroma.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" frame/format/0067_normalize_din99d_chroma || {
    echo "not ok 1 - DIN99d normalization maps and clips signed chroma"
    exit 0
}
echo "ok 1 - DIN99d normalization maps and clips signed chroma"
exit 0

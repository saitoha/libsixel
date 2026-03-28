#!/bin/sh
set -eux

SIXEL_HDR_CASE_LABEL='hdr matrix numeric case=185 target=gamma gamma=2.2 primaries=bt2020 ev=2 tonemap=none cms=none fallback=srgb'
SIXEL_HDR_TARGET='gamma'
SIXEL_HDR_GAMMA='2.2'
SIXEL_HDR_PRIMARIES='bt2020'
SIXEL_HDR_EXPOSURE_EV='2'
SIXEL_HDR_TONEMAP='none'
SIXEL_HDR_CMS_ENGINE='none'
SIXEL_HDR_FALLBACK_PROFILE='srgb'

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    --env "SIXEL_LOADER_CMS_TARGET_COLORSPACE=${SIXEL_HDR_TARGET}" \
    --env "SIXEL_LOADER_PREFER_8BIT=0" \
    --env "SIXEL_TEST_HDR_CASE_GAMMA=${SIXEL_HDR_GAMMA}" \
    --env "SIXEL_TEST_HDR_CASE_PRIMARIES=${SIXEL_HDR_PRIMARIES}" \
    --env "SIXEL_TEST_HDR_CASE_EXPOSURE_EV=${SIXEL_HDR_EXPOSURE_EV}" \
    --env "SIXEL_TEST_HDR_CASE_TONEMAP=${SIXEL_HDR_TONEMAP}" \
    --env "SIXEL_TEST_HDR_CASE_CMS_ENGINE=${SIXEL_HDR_CMS_ENGINE}" \
    --env "SIXEL_TEST_HDR_CASE_FALLBACK_PROFILE=${SIXEL_HDR_FALLBACK_PROFILE}" \
    --env "SIXEL_TEST_HDR_NUMERIC_SINGLE_CASE=1" \
    "loader/0014_loader_builtin_pixelformat" || {
    echo "not ok 1 - loader/0014_loader_builtin_pixelformat (${SIXEL_HDR_CASE_LABEL})"
    exit 0
}

echo "ok 1 - loader/0014_loader_builtin_pixelformat (${SIXEL_HDR_CASE_LABEL})"
exit 0

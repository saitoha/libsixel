#!/bin/sh
set -eux

SIXEL_HDR_CASE_LABEL='hdr matrix numeric case=156 target=gamma gamma=2.2 primaries=none ev=0 tonemap=reinhard cms=none fallback=linear-srgb'
SIXEL_HDR_TARGET='gamma'
SIXEL_HDR_GAMMA='2.2'
SIXEL_HDR_PRIMARIES='none'
SIXEL_HDR_EXPOSURE_EV='0'
SIXEL_HDR_TONEMAP='reinhard'
SIXEL_HDR_CMS_ENGINE='none'
SIXEL_HDR_FALLBACK_PROFILE='linear-srgb'

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

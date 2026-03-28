#!/bin/sh

SIXEL_HDR_CASE_ID='023'
SIXEL_HDR_CASE_LABEL='hdr matrix numeric case=023 target=linear gamma=none primaries=none ev=2 tonemap=reinhard cms=auto fallback=srgb'
SIXEL_HDR_TARGET='linear'
SIXEL_HDR_GAMMA='none'
SIXEL_HDR_PRIMARIES='none'
SIXEL_HDR_EXPOSURE_EV='2'
SIXEL_HDR_TONEMAP='reinhard'
SIXEL_HDR_CMS_ENGINE='auto'
SIXEL_HDR_FALLBACK_PROFILE='srgb'
. "${TOP_SRCDIR}/tests/loader/builtin/hdr_matrix_numeric_case_common.sh"

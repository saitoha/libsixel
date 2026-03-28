#!/bin/sh

SIXEL_HDR_CASE_ID='011'
SIXEL_HDR_CASE_LABEL='hdr matrix numeric case=011 target=linear gamma=none primaries=none ev=0 tonemap=none cms=auto fallback=srgb'
SIXEL_HDR_TARGET='linear'
SIXEL_HDR_GAMMA='none'
SIXEL_HDR_PRIMARIES='none'
SIXEL_HDR_EXPOSURE_EV='0'
SIXEL_HDR_TONEMAP='none'
SIXEL_HDR_CMS_ENGINE='auto'
SIXEL_HDR_FALLBACK_PROFILE='srgb'
. "${TOP_SRCDIR}/tests/loader/builtin/hdr_matrix_numeric_case_common.sh"

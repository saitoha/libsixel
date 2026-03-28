#!/bin/sh

SIXEL_HDR_CASE_ID='048'
SIXEL_HDR_CASE_LABEL='hdr matrix numeric case=048 target=linear gamma=2.2 primaries=none ev=-2 tonemap=none cms=none fallback=linear-srgb'
SIXEL_HDR_TARGET='linear'
SIXEL_HDR_GAMMA='2.2'
SIXEL_HDR_PRIMARIES='none'
SIXEL_HDR_EXPOSURE_EV='-2'
SIXEL_HDR_TONEMAP='none'
SIXEL_HDR_CMS_ENGINE='none'
SIXEL_HDR_FALLBACK_PROFILE='linear-srgb'
. "${TOP_SRCDIR}/tests/loader/builtin/hdr_matrix_numeric_case_common.sh"

#!/bin/sh

SIXEL_HDR_CASE_ID='009'
SIXEL_HDR_CASE_LABEL='hdr matrix lsqa case=009 target=gamma ev=-2 tonemap=reinhard cms=none fallback=linear-srgb'
SIXEL_HDR_TARGET='gamma'
SIXEL_HDR_EXPOSURE_EV='-2'
SIXEL_HDR_TONEMAP='reinhard'
SIXEL_HDR_CMS_ENGINE='none'
SIXEL_HDR_FALLBACK_PROFILE='linear-srgb'
. "${TOP_SRCDIR}/tests/loader/builtin/hdr_matrix_lsqa_case_common.sh"

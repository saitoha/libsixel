#!/bin/sh

SIXEL_HDR_CASE_ID='173'
SIXEL_HDR_CASE_LABEL='hdr matrix numeric case=173 target=gamma gamma=2.2 primaries=bt2020 ev=-2 tonemap=reinhard cms=none fallback=srgb'
SIXEL_HDR_TARGET='gamma'
SIXEL_HDR_GAMMA='2.2'
SIXEL_HDR_PRIMARIES='bt2020'
SIXEL_HDR_EXPOSURE_EV='-2'
SIXEL_HDR_TONEMAP='reinhard'
SIXEL_HDR_CMS_ENGINE='none'
SIXEL_HDR_FALLBACK_PROFILE='srgb'
. "${TOP_SRCDIR}/tests/loader/builtin/hdr_matrix_numeric_case_common.sh"

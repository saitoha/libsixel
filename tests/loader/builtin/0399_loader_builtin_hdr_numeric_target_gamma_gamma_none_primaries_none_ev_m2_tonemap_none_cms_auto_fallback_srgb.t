#!/bin/sh

SIXEL_HDR_CASE_ID='099'
SIXEL_HDR_CASE_LABEL='hdr matrix numeric case=099 target=gamma gamma=none primaries=none ev=-2 tonemap=none cms=auto fallback=srgb'
SIXEL_HDR_TARGET='gamma'
SIXEL_HDR_GAMMA='none'
SIXEL_HDR_PRIMARIES='none'
SIXEL_HDR_EXPOSURE_EV='-2'
SIXEL_HDR_TONEMAP='none'
SIXEL_HDR_CMS_ENGINE='auto'
SIXEL_HDR_FALLBACK_PROFILE='srgb'
. "${TOP_SRCDIR}/tests/loader/builtin/hdr_matrix_numeric_case_common.sh"

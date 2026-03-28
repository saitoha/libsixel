#!/bin/sh

SIXEL_HDR_CASE_ID='006'
SIXEL_HDR_CASE_LABEL='hdr matrix lsqa case=006 target=linear ev=-2 tonemap=none cms=auto fallback=srgb'
SIXEL_HDR_TARGET='linear'
SIXEL_HDR_EXPOSURE_EV='-2'
SIXEL_HDR_TONEMAP='none'
SIXEL_HDR_CMS_ENGINE='auto'
SIXEL_HDR_FALLBACK_PROFILE='srgb'
. "${TOP_SRCDIR}/tests/loader/builtin/hdr_matrix_lsqa_case_common.sh"

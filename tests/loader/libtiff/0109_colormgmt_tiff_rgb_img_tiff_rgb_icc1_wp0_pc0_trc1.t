#!/bin/sh
# TAP test: libtiff colormgmt parity for rgb/img_tiff_rgb_icc1_wp0_pc0_trc1.tiff

set -eux

CASE_SPACE=rgb
CASE_NAME=img_tiff_rgb_icc1_wp0_pc0_trc1
LSQA_FLOOR=0.996
. "${TOP_SRCDIR}/tests/_lib/sh/colormgmt_tiff_case.sh"

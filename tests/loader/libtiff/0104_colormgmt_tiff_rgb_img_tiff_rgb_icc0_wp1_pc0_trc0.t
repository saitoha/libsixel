#!/bin/sh
# TAP test: libtiff colormgmt parity for rgb/img_tiff_rgb_icc0_wp1_pc0_trc0.tiff

set -eux

CASE_SPACE=rgb
CASE_NAME=img_tiff_rgb_icc0_wp1_pc0_trc0
LSQA_FLOOR=0.999
. "${TOP_SRCDIR}/tests/_lib/sh/colormgmt_tiff_case.sh"

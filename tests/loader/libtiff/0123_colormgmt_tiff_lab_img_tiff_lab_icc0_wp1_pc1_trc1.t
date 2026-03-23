#!/bin/sh
# TAP test: libtiff colormgmt parity for lab/img_tiff_lab_icc0_wp1_pc1_trc1.tiff

set -eux

CASE_SPACE=lab
CASE_NAME=img_tiff_lab_icc0_wp1_pc1_trc1
LSQA_FLOOR=0.993
. "${TOP_SRCDIR}/tests/_lib/sh/colormgmt_tiff_case.sh"

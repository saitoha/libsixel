#!/bin/sh

set -e

original="${1}"
[ -n "${original}" ]

sixel="${2}"
[ -n "${sixel}" ]

dir="$(dirname "${0}")"
src_topdir="${dir}/.."
imgname="$(basename ${sixel})"

converters/sixel2png ${sixel} |
"${src_topdir}"/tools/evaluate.py --ref "${original}" --prefix=imagemagick-"${imgname}"


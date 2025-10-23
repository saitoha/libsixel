#!/bin/sh

set -e

dir="$(dirname "${0}")"
src_topdir="${dir}/.."
in="${1}"
imgname="$(basename ${in})"

magick "${in}" -dither fs six:-            |
converters/sixel2png                       |
"${src_topdir}"/tools/evaluate.py --ref "${in}" --prefix=imagemagick-"${imgname}"

converters/img2sixel "${in}"               |
converters/sixel2png                       |
"${src_topdir}"/tools/evaluate.py --ref "${in}" --prefix=libsixel-"${imgname}"

chafa -f sixel --dither diffusion "${in}"  |
converters/sixel2png                       |
"${src_topdir}"/tools/evaluate.py --ref "${in}" --prefix=chafa-"${imgname}"

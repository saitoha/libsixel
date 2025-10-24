#!/bin/sh

set -e

dir="$(dirname "${0}")"
src_topdir="${dir}/.."
in="${1}"

magick "${in}" -dither fs six:-            |
"${src_topdir}"/tools/evaluate.sh "${in}"

converters/img2sixel -dlso2 "${in}"        |
"${src_topdir}"/tools/evaluate.sh "${in}"

chafa -f sixel --dither diffusion "${in}"  |
"${src_topdir}"/tools/evaluate.sh "${in}"

#!/bin/sh
set -eu

. "$(dirname "$0")/lib.sh"

require_img2sixel

if [ "${HAVE_PNG:-0}" != "1" ]; then
  skip 'PNG support disabled'
fi

printf '[test9] various PNG\n'

basic_dir=$TOP_SRC_DIR/images/pngsuite/basic
background_dir=$TOP_SRC_DIR/images/pngsuite/background

basic_files="\
  basn0g01.png basn0g02.png basn0g04.png basn0g08.png basn0g16.png \
  basn3p01.png basn3p02.png basn3p04.png basn3p08.png basn4a08.png \
  basn4a16.png basn6a08.png basn6a16.png"

for f in $basic_files; do
  img2sixel "$basic_dir/$f" >/dev/null
  img2sixel -w32 "$basic_dir/$f" >/dev/null
  img2sixel -c16x16+8+8 "$basic_dir/$f" >/dev/null
done

background_files="\
  bgai4a08.png bgai4a16.png bgan6a08.png bgan6a16.png bgbn4a08.png \
  bggn4a16.png bgwn6a08.png bgyn6a16.png"

for f in $background_files; do
  img2sixel "$background_dir/$f" >/dev/null
  img2sixel -B#fff "$background_dir/$f" >/dev/null
  img2sixel -w32 -B#fff "$background_dir/$f" >/dev/null
done

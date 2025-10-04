#!/bin/sh
set -eu

. "$(dirname "$0")/lib.sh"

require_img2sixel

printf '[test14] documentation\n'

options1=$TMP_DIR/options1.txt
options2=$TMP_DIR/options2.txt
options3=$TMP_DIR/options3.txt
options4=$TMP_DIR/options4.txt

img2sixel -H | grep -E '^[[:space:]]*-' |
  sed 's/^[[:space:]]*//' | cut -f1 -d' ' | cut -f1 -d, >"$options1"
grep '^\.B' "$SRC_DIR/img2sixel.1" |
  cut -f2 -d' ' | grep '^\\' | tr -d '\\,' >"$options2"
grep ' --' "$SRC_DIR/shell-completion/bash/img2sixel" |
  grep -v "' " | sed 's/.* \(-.\) .*/\1/' >"$options3"
grep '{-' "$SRC_DIR/shell-completion/zsh/_img2sixel" |
  cut -f1 -d, | cut -f2 -d'{' >"$options4"

if command -v diff >/dev/null 2>&1; then
  diff "$options1" "$options2"
  diff "$options2" "$options3"
  diff "$options3" "$options4"
fi

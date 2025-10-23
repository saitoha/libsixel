#!/bin/sh

in="${1-/dev/stdin}"

printf '\033]1337;File=inline=1:'
base64 -w0 < "${in}"
printf '\007\n'

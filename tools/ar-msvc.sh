#!/usr/bin/env bash
out="$1"; shift
exec lib -nologo "-OUT:${out}" "$@"

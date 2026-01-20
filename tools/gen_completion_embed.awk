#!/usr/bin/env awk -f
# Generate completion_embed.h from bash/zsh completion sources.
#
# ASCII pipeline summary:
#
#   bash/img2sixel ----\
#                       >--[gen_completion_embed.awk]--> completion_embed.h
#   zsh/_img2sixel ----/
#
# The script accepts two arguments:
#   1. Path to the bash completion file
#   2. Path to the zsh completion file
# It writes the resulting header to STDOUT so that callers can redirect it.
#
# Emit byte arrays to avoid triggering -Woverlength-strings on
# strict ISO C99 compilers while preserving raw file contents.

function emit_byte(byte,
    count_per_line)
{
        count_per_line = 12;

        if (byte_count == 0) {
                printf "    "
        }

        printf "0x%s", byte
        byte_count++

        if (byte_count >= count_per_line) {
                printf ",\n"
                byte_count = 0
        } else {
                printf ", "
        }
}

function emit_bytes(path, symbol,
    cmd, line, fields, idx, field_count)
{
        print "static const unsigned char " symbol "[] = {"

        byte_count = 0
        cmd = "od -An -tx1 -v '" path "'"
        while ((cmd | getline line) > 0) {
                field_count = split(line, fields, /[ \t]+/)
                for (idx = 1; idx <= field_count; idx++) {
                        if (fields[idx] == "") {
                                continue
                        }
                        emit_byte(fields[idx])
                }
        }
        close(cmd)

        emit_byte("00")
        if (byte_count != 0) {
                printf "\n"
        }

        print "};"
        print ""
}

BEGIN {
        if (ARGC != 3) {
                print "Usage: gen_completion_embed.awk BASH ZSH" > \
                    "/dev/stderr"
                exit 1
        }

        bash_path = ARGV[1]
        zsh_path = ARGV[2]
        ARGV[1] = ""
        ARGV[2] = ""

        print "/* auto-generated; do not edit */"
        print ""

        emit_bytes(bash_path, "img2sixel_bash_completion")
        emit_bytes(zsh_path, "img2sixel_zsh_completion")

        exit 0
}

#!/usr/bin/env ruby

require "mkmf"
if have_header('sixel.h') and have_library('sixel')
  create_makefile("libsixel/libsixel")
end

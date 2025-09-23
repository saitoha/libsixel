#!/usr/bin/env ruby
#
# An example usage of libsixel Output/Dither object
#
# Author: Hayaki Saito <saitoha@me.com>
#
# I declare this program is in Public Domain (CC0 - "No Rights Reserved"),
# This file is offered AS-IS, without any warranty.
#
$LOAD_PATH.unshift File.expand_path('../../ruby/lib', __dir__)
require 'libsixel'

width = 256
height = 64

# Create a simple RGB gradient buffer (RGB888)
buf = String.new(capacity: width*height*3)
height.times do |y|
  width.times do |x|
    r = x & 0xFF
    g = (y * 4) & 0xFF
    b = ((x + y) * 2) & 0xFF
    buf << r.chr << g.chr << b.chr
  end
end

out = Output.new(write_proc: ->(chunk, _) { STDOUT.write(chunk) })
dither = Dither.new(ncolors: 256)

pf = Libsixel::API::SIXEL_PIXELFORMAT_RGB888 rescue 0x1003
depth = Libsixel::Helper.compute_depth(pf)

status = Libsixel::API.sixel_encode(buf, width, height, depth, dither.ptr, out.ptr)
raise Libsixel::API::Err.message(status) if Libsixel::API.failed?(status)

# emacs Local Variables:
# emacs mode: ruby
# emacs tab-width: 2
# emacs indent-tabs-mode: nil
# emacs ruby-indent-level: 2
# emacs End:
# vim: set expandtab ts=2 sts=2 sw=2 :
# EOF

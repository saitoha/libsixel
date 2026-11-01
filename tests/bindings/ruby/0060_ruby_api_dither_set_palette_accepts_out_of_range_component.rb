#!/usr/bin/env ruby
# frozen_string_literal: true

puts '1..1'

begin
  require 'libsixel'

  dither = Dither.new(ncolors: 2)
  dither.set_palette([0, 0, 0, 256, 0, 0])
  puts 'ok 1 - dither set_palette accepts out-of-range component in current Ruby path'
rescue StandardError => e
  puts 'not ok 1 - dither set_palette out-of-range acceptance check failed'
  puts "# #{e.class}: #{e.message}"
end

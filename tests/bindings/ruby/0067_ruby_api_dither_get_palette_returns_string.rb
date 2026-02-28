#!/usr/bin/env ruby
# frozen_string_literal: true

puts '1..1'

begin
  require 'libsixel'

  dither = Dither.new(ncolors: 2)
  dither.set_palette([1, 2, 3, 4, 5, 6])
  palette = dither.palette

  if palette.is_a?(String) && palette.bytesize >= 6
    puts 'ok 1 - dither get_palette returns a byte string payload'
  else
    puts 'not ok 1 - dither get_palette did not return expected byte string payload'
  end
rescue StandardError => e
  puts 'not ok 1 - dither get_palette return-type check failed'
  puts "# #{e.class}: #{e.message}"
end

#!/usr/bin/env ruby
# frozen_string_literal: true

puts '1..1'

begin
  require 'libsixel'

  dither = Dither.new(ncolors: 2)
  expected_palette = [1, 2, 3, 4, 5, 6]
  dither.set_palette(expected_palette)
  actual_palette = dither.palette.bytes
  dither.unref

  if actual_palette[0, expected_palette.length] == expected_palette
    puts "ok 1 - dither palette getter/setter APIs are callable (#{actual_palette.length} bytes)"
  else
    puts 'not ok 1 - dither palette getter returned unexpected data'
  end
rescue StandardError => e
  puts 'not ok 1 - dither palette API check failed'
  puts "# #{e.class}: #{e.message}"
end

#!/usr/bin/env ruby
# frozen_string_literal: true

puts '1..1'

begin
  require 'libsixel'

  dither = Dither.new(ncolors: 2)
  dither.set_palette([0, 0, 0, 255, 255, 255])
  palette = dither.palette
  dither.unref

  if palette.is_a?(String)
    puts 'ok 1 - dither set_palette and palette getter are callable'
  else
    puts 'not ok 1 - dither palette getter did not return String payload'
  end
rescue StandardError => e
  puts 'not ok 1 - dither palette roundtrip check failed'
  puts "# #{e.class}: #{e.message}"
end

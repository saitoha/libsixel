#!/usr/bin/env ruby
# frozen_string_literal: true

puts '1..1'

begin
  require 'libsixel'

  pixels = [
    255, 0, 0,
    0, 255, 0,
    0, 0, 255,
    255, 255, 255
  ]

  dither = Dither.new(ncolors: 16)
  dither.initialize_palette(
    pixels,
    width: 2,
    height: 2,
    pixelformat: Libsixel::API::SIXEL_PIXELFORMAT_RGB888,
    method_for_largest: Libsixel::API::SIXEL_LARGE_AUTO,
    method_for_rep: Libsixel::API::SIXEL_REP_AUTO,
    quality_mode: Libsixel::API::SIXEL_QUALITY_AUTO
  )
  histogram = dither.num_histogram_colors
  dither.unref

  if histogram > 0
    puts "ok 1 - dither initialize updated histogram state (#{histogram})"
  else
    puts 'not ok 1 - dither initialize did not update histogram state'
  end
rescue StandardError => e
  puts 'not ok 1 - dither initialize check failed'
  puts "# #{e.class}: #{e.message}"
end

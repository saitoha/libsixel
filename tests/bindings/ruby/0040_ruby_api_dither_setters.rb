#!/usr/bin/env ruby
# frozen_string_literal: true

puts '1..1'

begin
  require 'libsixel'

  dither = Dither.new(ncolors: 16)
  dither.set_diffusion_type(Libsixel::API::SIXEL_DIFFUSE_ATKINSON)
  dither.set_diffusion_scan(Libsixel::API::SIXEL_SCAN_SERPENTINE)
  dither.set_complexion_score(1)
  dither.set_body_only(0)
  dither.set_optimize_palette(1)
  dither.set_pixelformat(Libsixel::API::SIXEL_PIXELFORMAT_RGB888)
  dither.set_transparent(0)
  dither.unref

  puts 'ok 1 - dither setter APIs accept expected argument values'
rescue StandardError => e
  puts 'not ok 1 - dither setter API check failed'
  puts "# #{e.class}: #{e.message}"
end

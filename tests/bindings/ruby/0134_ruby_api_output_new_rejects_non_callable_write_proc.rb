#!/usr/bin/env ruby
# frozen_string_literal: true

puts '1..1'

begin
  require 'libsixel'

  output = Output.new(write_proc: 123)
  dither = Libsixel::API.sixel_dither_get(Libsixel::API::SIXEL_BUILTIN_XTERM256)
  pixels = [255, 0, 0].pack('C*')
  depth = Libsixel::API.sixel_helper_compute_depth(Libsixel::API::SIXEL_PIXELFORMAT_RGB888)

  begin
    Libsixel::API.sixel_encode(pixels, 1, 1, depth, dither, output.ptr)
    puts 'not ok 1 - output accepted non-callable write_proc during callback'
  rescue NoMethodError, TypeError, RuntimeError
    puts 'ok 1 - output raises when write_proc is non-callable'
  end
rescue StandardError => e
  puts 'not ok 1 - output non-callable write_proc check failed'
  puts "# #{e.class}: #{e.message}"
end

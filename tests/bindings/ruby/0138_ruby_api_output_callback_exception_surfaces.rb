#!/usr/bin/env ruby
# frozen_string_literal: true

puts '1..1'

begin
  require 'libsixel'

  output = Output.new(write_proc: proc { |_chunk, _priv| raise 'callback boom' })
  dither = Libsixel::API.sixel_dither_get(Libsixel::API::SIXEL_BUILTIN_XTERM256)
  pixels = [255, 0, 0].pack('C*')
  depth = Libsixel::API.sixel_helper_compute_depth(Libsixel::API::SIXEL_PIXELFORMAT_RGB888)

  begin
    Libsixel::API.sixel_encode(pixels, 1, 1, depth, dither, output.ptr)
    puts 'not ok 1 - output callback exception did not surface'
  rescue RuntimeError
    puts 'ok 1 - output callback exception surfaces as runtime error'
  rescue StandardError
    puts 'ok 1 - output callback exception surfaces as ruby exception'
  end
rescue StandardError => e
  puts 'not ok 1 - output callback exception check failed'
  puts "# #{e.class}: #{e.message}"
end

#!/usr/bin/env ruby
# frozen_string_literal: true

puts '1..1'

begin
  require 'libsixel'

  callback_error = Class.new(StandardError)
  output = Output.new(write_proc: proc { |_chunk, _priv| raise callback_error, 'custom callback failure' })
  dither = Libsixel::API.sixel_dither_get(Libsixel::API::SIXEL_BUILTIN_XTERM256)
  pixels = [255, 0, 0].pack('C*')
  depth = Libsixel::API.sixel_helper_compute_depth(Libsixel::API::SIXEL_PIXELFORMAT_RGB888)

  begin
    Libsixel::API.sixel_encode(pixels, 1, 1, depth, dither, output.ptr)
    puts 'not ok 1 - custom callback exception did not surface'
  rescue callback_error
    puts 'ok 1 - custom callback exception type is preserved'
  rescue StandardError => e
    puts 'not ok 1 - callback exception type was not preserved'
    puts "# #{e.class}: #{e.message}"
  end
rescue StandardError => e
  puts 'not ok 1 - custom callback exception type check failed'
  puts "# #{e.class}: #{e.message}"
end

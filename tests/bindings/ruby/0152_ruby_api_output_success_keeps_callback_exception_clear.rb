#!/usr/bin/env ruby
# frozen_string_literal: true

puts '1..1'

begin
  require 'libsixel'

  called = 0
  output = Output.new(write_proc: proc { |_chunk, _priv| called += 1 })
  dither = Libsixel::API.sixel_dither_get(Libsixel::API::SIXEL_BUILTIN_XTERM256)
  pixels = [255, 0, 0].pack('C*')
  depth = Libsixel::API.sixel_helper_compute_depth(Libsixel::API::SIXEL_PIXELFORMAT_RGB888)

  first = Libsixel::API.sixel_encode(pixels, 1, 1, depth, dither, output.ptr)
  second = Libsixel::API.sixel_encode(pixels, 1, 1, depth, dither, output.ptr)

  if Libsixel::API.failed?(first)
    puts 'not ok 1 - first output encode failed unexpectedly'
  elsif Libsixel::API.failed?(second)
    puts 'not ok 1 - second output encode failed unexpectedly'
  elsif called < 2
    puts 'not ok 1 - successful callback was not invoked twice'
  else
    puts 'ok 1 - successful output callback remains clear across repeated encode'
  end
rescue StandardError => e
  puts 'not ok 1 - successful output callback repeat check failed'
  puts "# #{e.class}: #{e.message}"
end

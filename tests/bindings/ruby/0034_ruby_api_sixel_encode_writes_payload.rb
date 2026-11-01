#!/usr/bin/env ruby
# frozen_string_literal: true

puts '1..1'

begin
  require 'libsixel'

  chunks = []
  output = Output.new(write_proc: proc { |chunk, _priv| chunks << chunk })
  dither = Libsixel::API.sixel_dither_get(Libsixel::API::SIXEL_BUILTIN_XTERM256)

  raise RuntimeError, 'sixel_dither_get returned null' if dither.nil? || dither.to_i == 0

  pixels = [
    255, 0, 0,
    0, 255, 0,
    0, 0, 255,
    255, 255, 255
  ].pack('C*')
  depth = Libsixel::API.sixel_helper_compute_depth(Libsixel::API::SIXEL_PIXELFORMAT_RGB888)
  status = Libsixel::API.sixel_encode(pixels, 2, 2, depth, dither, output.ptr)

  if Libsixel::API.failed?(status)
    puts 'not ok 1 - sixel_encode returned failure status'
  else
    payload = chunks.join
    if payload.start_with?("\eP")
      puts 'ok 1 - sixel_encode writes sixel payload through output callback'
    else
      puts 'not ok 1 - sixel_encode payload missing sixel introducer'
    end
  end
rescue StandardError => e
  puts 'not ok 1 - sixel_encode callback payload check failed'
  puts "# #{e.class}: #{e.message}"
end

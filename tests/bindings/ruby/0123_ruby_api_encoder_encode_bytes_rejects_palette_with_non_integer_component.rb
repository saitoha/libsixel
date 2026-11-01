#!/usr/bin/env ruby
# frozen_string_literal: true

puts '1..1'

begin
  require 'libsixel'

  encoder = Encoder.new
  begin
    encoder.encode_bytes(
      [255, 0, 0, 0, 0, 0, 0, 0].pack('C*'),
      width: 1,
      height: 1,
      pixelformat: Libsixel::API::SIXEL_PIXELFORMAT_RGB888,
      palette: [255, 'x', 0]
    )
    puts 'not ok 1 - encoder accepted palette with non-integer component'
  rescue ArgumentError, TypeError, RuntimeError
    puts 'ok 1 - encoder encode_bytes rejects palette with non-integer component'
  end
rescue StandardError => e
  puts 'not ok 1 - encoder non-integer palette-component rejection check failed'
  puts "# #{e.class}: #{e.message}"
end

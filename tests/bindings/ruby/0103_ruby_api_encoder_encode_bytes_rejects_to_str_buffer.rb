#!/usr/bin/env ruby
# frozen_string_literal: true

puts '1..1'

begin
  require 'libsixel'

  encoder = Encoder.new
  buffer_like = Class.new do
    def to_str
      [
        255, 0, 0,
        0, 255, 0,
        0, 0, 255,
        255, 255, 255
      ].pack('C*')
    end
  end.new

  begin
    encoder.encode_bytes(
      buffer_like,
      width: 2,
      height: 2,
      pixelformat: Libsixel::API::SIXEL_PIXELFORMAT_RGB888
    )
    puts 'ok 1 - encoder encode_bytes accepts to_str buffer object'
  rescue TypeError
    puts 'not ok 1 - encoder encode_bytes rejected to_str buffer object'
  end
rescue StandardError => e
  puts 'not ok 1 - encoder encode_bytes to_str buffer acceptance check failed'
  puts "# #{e.class}: #{e.message}"
end

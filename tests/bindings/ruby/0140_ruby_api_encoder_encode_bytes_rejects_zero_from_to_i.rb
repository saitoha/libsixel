#!/usr/bin/env ruby
# frozen_string_literal: true

puts '1..1'

begin
  require 'libsixel'

  width_like = Class.new do
    def to_i
      0
    end
  end.new

  encoder = Encoder.new
  begin
    encoder.encode_bytes(
      [255, 0, 0].pack('C*'),
      width: width_like,
      height: 1,
      pixelformat: Libsixel::API::SIXEL_PIXELFORMAT_RGB888
    )
    puts 'not ok 1 - encode_bytes accepted zero width from to_i coercion'
  rescue ArgumentError
    puts 'ok 1 - encode_bytes rejects zero width after to_i coercion'
  end
rescue StandardError => e
  puts 'not ok 1 - encode_bytes to_i coercion boundary check failed'
  puts "# #{e.class}: #{e.message}"
end

#!/usr/bin/env ruby
# frozen_string_literal: true

puts '1..1'

begin
  require 'libsixel'

  encoder = Encoder.new
  pixels = [255, 0, 0].pack('C*')

  begin
    encoder.encode_bytes(
      pixels,
      width: 1,
      pixelformat: Libsixel::API::SIXEL_PIXELFORMAT_RGB888
    )
    puts 'not ok 1 - encode_bytes accepted missing height keyword'
  rescue ArgumentError
    puts 'ok 1 - encode_bytes requires height keyword'
  end
rescue StandardError => e
  puts 'not ok 1 - encode_bytes height keyword check failed'
  puts "# #{e.class}: #{e.message}"
end

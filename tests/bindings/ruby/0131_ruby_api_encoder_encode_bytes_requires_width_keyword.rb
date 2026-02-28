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
      height: 1,
      pixelformat: Libsixel::API::SIXEL_PIXELFORMAT_RGB888
    )
    puts 'not ok 1 - encode_bytes accepted missing width keyword'
  rescue ArgumentError
    puts 'ok 1 - encode_bytes requires width keyword'
  end
rescue StandardError => e
  puts 'not ok 1 - encode_bytes width keyword check failed'
  puts "# #{e.class}: #{e.message}"
end

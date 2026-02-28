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
      palette: 'bad-palette'
    )
    puts 'not ok 1 - encoder encode_bytes accepted string palette input'
  rescue ArgumentError, TypeError, RuntimeError, NoMethodError
    puts 'ok 1 - encoder encode_bytes rejects string palette input'
  end
rescue StandardError => e
  puts 'not ok 1 - encoder string palette rejection check failed'
  puts "# #{e.class}: #{e.message}"
end

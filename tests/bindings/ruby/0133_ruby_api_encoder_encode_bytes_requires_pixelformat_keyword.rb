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
      height: 1
    )
    puts 'not ok 1 - encode_bytes accepted missing pixelformat keyword'
  rescue ArgumentError
    puts 'ok 1 - encode_bytes requires pixelformat keyword'
  end
rescue StandardError => e
  puts 'not ok 1 - encode_bytes pixelformat keyword check failed'
  puts "# #{e.class}: #{e.message}"
end

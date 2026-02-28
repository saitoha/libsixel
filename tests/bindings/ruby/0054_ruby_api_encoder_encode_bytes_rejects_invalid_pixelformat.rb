#!/usr/bin/env ruby
# frozen_string_literal: true

puts '1..1'

begin
  require 'libsixel'

  encoder = Encoder.new
  begin
    encoder.encode_bytes(
      "\x00\x00\x00".b,
      width: 1,
      height: 1,
      pixelformat: -1
    )
    puts 'not ok 1 - encoder encode_bytes accepted invalid pixelformat'
  rescue ArgumentError, RuntimeError
    puts 'ok 1 - encoder encode_bytes rejects invalid pixelformat'
  end
rescue StandardError => e
  puts 'not ok 1 - encoder invalid-pixelformat rejection check failed'
  puts "# #{e.class}: #{e.message}"
end

#!/usr/bin/env ruby
# frozen_string_literal: true

puts '1..1'

begin
  require 'libsixel'

  decoder = Decoder.new
  begin
    decoder.decode([100, 117, 109, 109, 121, 46, 115, 105, 120])
    puts 'not ok 1 - decoder decode accepted bytearray-like infile input'
  rescue ArgumentError, TypeError, RuntimeError
    puts 'ok 1 - decoder decode rejects bytearray-like infile input'
  end
rescue StandardError => e
  puts 'not ok 1 - decoder bytearray-like infile rejection check failed'
  puts "# #{e.class}: #{e.message}"
end

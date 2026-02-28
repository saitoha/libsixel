#!/usr/bin/env ruby
# frozen_string_literal: true

puts '1..1'

begin
  require 'libsixel'

  decoder = Decoder.new
  begin
    decoder.setopt([105], 'dummy.png')
    puts 'not ok 1 - decoder accepted bytearray-like option flag input'
  rescue ArgumentError, TypeError, RuntimeError
    puts 'ok 1 - decoder setopt rejects bytearray-like option flag input'
  end
rescue StandardError => e
  puts 'not ok 1 - decoder bytearray-like option flag rejection check failed'
  puts "# #{e.class}: #{e.message}"
end

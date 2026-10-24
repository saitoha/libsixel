#!/usr/bin/env ruby
# frozen_string_literal: true

puts '1..1'

begin
  require 'libsixel'

  begin
    Libsixel.set_encoder_threads([97, 108, 108])
    puts 'not ok 1 - set_encoder_threads accepted bytearray-like all input'
  rescue ArgumentError
    puts 'ok 1 - set_encoder_threads rejects bytearray-like all input'
  end
rescue StandardError => e
  puts 'not ok 1 - set_encoder_threads bytearray-like all rejection check failed'
  puts "# #{e.class}: #{e.message}"
end

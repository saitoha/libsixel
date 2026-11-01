#!/usr/bin/env ruby
# frozen_string_literal: true

puts '1..1'

begin
  require 'libsixel'

  begin
    Libsixel.set_encoder_threads('0'.b)
    puts 'not ok 1 - set_encoder_threads accepted zero-byte input'
  rescue ArgumentError
    puts 'ok 1 - set_encoder_threads rejects zero-byte input'
  end
rescue StandardError => e
  puts 'not ok 1 - set_encoder_threads zero-byte rejection check failed'
  puts "# #{e.class}: #{e.message}"
end

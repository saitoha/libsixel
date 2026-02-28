#!/usr/bin/env ruby
# frozen_string_literal: true

puts '1..1'

begin
  require 'libsixel'

  begin
    Libsixel.set_threads([97, 117, 116, 111])
    puts 'not ok 1 - set_threads accepted bytearray-like auto input'
  rescue ArgumentError
    puts 'ok 1 - set_threads rejects bytearray-like auto input'
  end
rescue StandardError => e
  puts 'not ok 1 - set_threads bytearray-like auto rejection check failed'
  puts "# #{e.class}: #{e.message}"
end

#!/usr/bin/env ruby
# frozen_string_literal: true

puts '1..1'

begin
  require 'libsixel'

  decoder = Decoder.new
  begin
    decoder.decode(12345)
    puts 'not ok 1 - decoder accepted integer infile input'
  rescue ArgumentError, TypeError, RuntimeError
    puts 'ok 1 - decoder decode rejects integer infile input'
  end
rescue StandardError => e
  puts 'not ok 1 - decoder integer infile rejection check failed'
  puts "# #{e.class}: #{e.message}"
end

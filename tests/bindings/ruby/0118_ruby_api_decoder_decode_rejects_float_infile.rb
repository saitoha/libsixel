#!/usr/bin/env ruby
# frozen_string_literal: true

puts '1..1'

begin
  require 'libsixel'

  decoder = Decoder.new
  begin
    decoder.decode(1.5)
    puts 'not ok 1 - decoder accepted float infile input'
  rescue ArgumentError, TypeError, RuntimeError
    puts 'ok 1 - decoder decode rejects float infile input'
  end
rescue StandardError => e
  puts 'not ok 1 - decoder float infile rejection check failed'
  puts "# #{e.class}: #{e.message}"
end

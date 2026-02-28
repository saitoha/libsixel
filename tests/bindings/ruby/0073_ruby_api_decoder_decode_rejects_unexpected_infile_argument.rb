#!/usr/bin/env ruby
# frozen_string_literal: true

puts '1..1'

begin
  require 'libsixel'

  decoder = Decoder.new
  begin
    decoder.decode('tests/data/inputs/snake_64.six')
    puts 'not ok 1 - decoder decode accepted unexpected infile argument'
  rescue ArgumentError
    puts 'ok 1 - decoder decode rejects unexpected infile argument'
  end
rescue StandardError => e
  puts 'not ok 1 - decoder decode infile-argument rejection check failed'
  puts "# #{e.class}: #{e.message}"
end

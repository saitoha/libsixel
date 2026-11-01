#!/usr/bin/env ruby
# frozen_string_literal: true

puts '1..1'

begin
  require 'libsixel'

  decoder = Decoder.new
  begin
    decoder.decode('dummy.six'.b)
    puts 'not ok 1 - decoder decode accepted bytes infile argument'
  rescue ArgumentError
    puts 'ok 1 - decoder decode rejects bytes infile argument'
  end
rescue StandardError => e
  puts 'not ok 1 - decoder decode bytes infile rejection check failed'
  puts "# #{e.class}: #{e.message}"
end

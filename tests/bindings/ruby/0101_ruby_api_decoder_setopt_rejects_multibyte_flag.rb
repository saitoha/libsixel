#!/usr/bin/env ruby
# frozen_string_literal: true

puts '1..1'

begin
  require 'libsixel'

  decoder = Decoder.new
  begin
    decoder.setopt('xx', 'dummy.png')
    puts 'not ok 1 - decoder accepted multi-byte option flag input'
  rescue ArgumentError, TypeError, RuntimeError
    puts 'ok 1 - decoder setopt rejects multi-byte option flag input'
  end
rescue StandardError => e
  puts 'not ok 1 - decoder multi-byte option flag rejection check failed'
  puts "# #{e.class}: #{e.message}"
end

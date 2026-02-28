#!/usr/bin/env ruby
# frozen_string_literal: true

puts '1..1'

begin
  require 'libsixel'

  decoder = Decoder.new
  decoder.setopt('o'.b, 'dummy.png')

  puts 'ok 1 - decoder accepts bytes option flag input'
rescue StandardError => e
  puts 'not ok 1 - decoder bytes option flag acceptance check failed'
  puts "# #{e.class}: #{e.message}"
end

#!/usr/bin/env ruby
# frozen_string_literal: true

puts '1..1'

begin
  require 'libsixel'

  decoder = Decoder.new

  begin
    decoder.setopt(nil, 'foo')
    puts 'not ok 1 - decoder accepted nil option flag'
  rescue RuntimeError, TypeError, ArgumentError
    puts 'ok 1 - decoder rejects nil option flag'
  end
rescue StandardError => e
  puts 'not ok 1 - decoder nil option flag rejection check failed'
  puts "# #{e.class}: #{e.message}"
end

#!/usr/bin/env ruby
# frozen_string_literal: true

puts '1..1'

begin
  require 'libsixel'

  decoder = Decoder.new

  begin
    decoder.setopt('xy', 'dummy.png')
    puts 'not ok 1 - decoder accepted multi-character option flag'
  rescue RuntimeError, TypeError, ArgumentError
    puts 'ok 1 - decoder rejects multi-character option flag'
  end
rescue StandardError => e
  puts 'not ok 1 - decoder multi-character flag rejection check failed'
  puts "# #{e.class}: #{e.message}"
end

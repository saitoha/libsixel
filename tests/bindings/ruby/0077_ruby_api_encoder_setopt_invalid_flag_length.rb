#!/usr/bin/env ruby
# frozen_string_literal: true

puts '1..1'

begin
  require 'libsixel'

  encoder = Encoder.new

  begin
    encoder.setopt('xy', '16')
    puts 'not ok 1 - encoder accepted multi-character option flag'
  rescue RuntimeError, TypeError, ArgumentError
    puts 'ok 1 - encoder rejects multi-character option flag'
  end
rescue StandardError => e
  puts 'not ok 1 - encoder multi-character flag rejection check failed'
  puts "# #{e.class}: #{e.message}"
end

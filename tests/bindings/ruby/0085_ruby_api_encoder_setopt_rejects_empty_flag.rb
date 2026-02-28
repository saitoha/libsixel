#!/usr/bin/env ruby
# frozen_string_literal: true

puts '1..1'

begin
  require 'libsixel'

  encoder = Encoder.new

  begin
    encoder.setopt('', '16')
    puts 'not ok 1 - encoder accepted empty option flag'
  rescue RuntimeError, TypeError, ArgumentError
    puts 'ok 1 - encoder rejects empty option flag'
  end
rescue StandardError => e
  puts 'not ok 1 - encoder empty option flag rejection check failed'
  puts "# #{e.class}: #{e.message}"
end

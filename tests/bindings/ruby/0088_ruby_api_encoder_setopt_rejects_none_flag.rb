#!/usr/bin/env ruby
# frozen_string_literal: true

puts '1..1'

begin
  require 'libsixel'

  encoder = Encoder.new

  begin
    encoder.setopt(nil, '16')
    puts 'not ok 1 - encoder accepted nil option flag'
  rescue RuntimeError, TypeError, ArgumentError
    puts 'ok 1 - encoder rejects nil option flag'
  end
rescue StandardError => e
  puts 'not ok 1 - encoder nil option flag rejection check failed'
  puts "# #{e.class}: #{e.message}"
end

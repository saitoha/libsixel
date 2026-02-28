#!/usr/bin/env ruby
# frozen_string_literal: true

puts '1..1'

begin
  require 'libsixel'

  encoder = Encoder.new
  begin
    encoder.setopt(-1, '16')
    puts 'not ok 1 - encoder accepted negative integer option flag'
  rescue RuntimeError, ArgumentError, TypeError
    puts 'ok 1 - encoder rejects negative integer option flag'
  end
rescue StandardError => e
  puts 'not ok 1 - encoder negative-flag rejection check failed'
  puts "# #{e.class}: #{e.message}"
end

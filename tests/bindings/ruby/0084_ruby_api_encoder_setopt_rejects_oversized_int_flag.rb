#!/usr/bin/env ruby
# frozen_string_literal: true

puts '1..1'

begin
  require 'libsixel'

  encoder = Encoder.new
  begin
    encoder.setopt(0x11_0000, '16')
    puts 'not ok 1 - encoder accepted oversized integer option flag'
  rescue RuntimeError, ArgumentError, TypeError
    puts 'ok 1 - encoder rejects oversized integer option flag'
  end
rescue StandardError => e
  puts 'not ok 1 - encoder oversized-flag rejection check failed'
  puts "# #{e.class}: #{e.message}"
end

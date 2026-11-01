#!/usr/bin/env ruby
# frozen_string_literal: true

puts '1..1'

begin
  require 'libsixel'

  encoder = Encoder.new
  begin
    encoder.encode('.'.dup.force_encoding(Encoding::ASCII_8BIT))
    puts 'not ok 1 - encoder accepted directory bytes filename input'
  rescue RuntimeError
    puts 'ok 1 - encoder encode rejects directory bytes filename input'
  end
rescue StandardError => e
  puts 'not ok 1 - encoder directory bytes filename rejection check failed'
  puts "# #{e.class}: #{e.message}"
end

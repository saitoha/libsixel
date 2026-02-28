#!/usr/bin/env ruby
# frozen_string_literal: true

puts '1..1'

begin
  require 'libsixel'

  encoder = Encoder.new

  begin
    encoder.encode('.')
    puts 'not ok 1 - encoder accepted directory path'
  rescue RuntimeError
    puts 'ok 1 - encoder rejects directory paths'
  end
rescue StandardError => e
  puts 'not ok 1 - encoder directory-path rejection check failed'
  puts "# #{e.class}: #{e.message}"
end

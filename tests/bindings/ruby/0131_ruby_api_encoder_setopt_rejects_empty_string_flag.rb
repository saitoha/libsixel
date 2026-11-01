#!/usr/bin/env ruby
# frozen_string_literal: true

puts '1..1'

begin
  require 'libsixel'

  encoder = Encoder.new
  begin
    encoder.setopt('', '16')
    puts 'not ok 1 - encoder setopt accepted empty string flag'
  rescue TypeError, RuntimeError, ArgumentError
    puts 'ok 1 - encoder setopt rejects empty string flag'
  end
rescue StandardError => e
  puts 'not ok 1 - encoder setopt empty-string flag check failed'
  puts "# #{e.class}: #{e.message}"
end

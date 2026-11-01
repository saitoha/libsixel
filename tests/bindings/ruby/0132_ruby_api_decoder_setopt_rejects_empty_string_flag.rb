#!/usr/bin/env ruby
# frozen_string_literal: true

puts '1..1'

begin
  require 'libsixel'

  decoder = Decoder.new
  begin
    decoder.setopt('', 'dummy.png')
    puts 'not ok 1 - decoder setopt accepted empty string flag'
  rescue TypeError, RuntimeError, ArgumentError
    puts 'ok 1 - decoder setopt rejects empty string flag'
  end
rescue StandardError => e
  puts 'not ok 1 - decoder setopt empty-string flag check failed'
  puts "# #{e.class}: #{e.message}"
end

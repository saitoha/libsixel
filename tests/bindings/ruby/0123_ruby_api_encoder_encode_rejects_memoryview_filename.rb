#!/usr/bin/env ruby
# frozen_string_literal: true

require 'stringio'

puts '1..1'

begin
  require 'libsixel'

  encoder = Encoder.new
  begin
    encoder.encode(StringIO.new('dummy.png'))
    puts 'not ok 1 - encoder encode accepted memoryview-like filename input'
  rescue ArgumentError, TypeError, RuntimeError
    puts 'ok 1 - encoder encode rejects memoryview-like filename input'
  end
rescue StandardError => e
  puts 'not ok 1 - encoder memoryview-like filename rejection check failed'
  puts "# #{e.class}: #{e.message}"
end

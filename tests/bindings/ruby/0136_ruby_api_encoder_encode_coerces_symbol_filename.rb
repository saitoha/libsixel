#!/usr/bin/env ruby
# frozen_string_literal: true

puts '1..1'

begin
  require 'libsixel'

  encoder = Encoder.new
  begin
    encoder.encode(:not_a_real_file)
    puts 'not ok 1 - encoder encode accepted symbol filename unexpectedly'
  rescue RuntimeError
    puts 'ok 1 - encoder encode coerces symbol filename and fails as path'
  rescue TypeError, ArgumentError
    puts 'not ok 1 - encoder encode did not coerce symbol filename'
  end
rescue StandardError => e
  puts 'not ok 1 - encoder symbol filename coercion check failed'
  puts "# #{e.class}: #{e.message}"
end

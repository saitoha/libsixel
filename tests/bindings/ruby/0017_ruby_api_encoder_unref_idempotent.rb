#!/usr/bin/env ruby
# frozen_string_literal: true

puts '1..1'

begin
  require 'libsixel'

  encoder = Encoder.new
  encoder.unref
  encoder.unref

  puts 'ok 1 - encoder unref is idempotent'
rescue StandardError => e
  puts 'not ok 1 - encoder unref idempotent check failed'
  puts "# #{e.class}: #{e.message}"
end

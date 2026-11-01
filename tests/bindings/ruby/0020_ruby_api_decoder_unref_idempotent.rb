#!/usr/bin/env ruby
# frozen_string_literal: true

puts '1..1'

begin
  require 'libsixel'

  decoder = Decoder.new
  decoder.unref
  decoder.unref

  puts 'ok 1 - decoder unref is idempotent'
rescue StandardError => e
  puts 'not ok 1 - decoder unref idempotent check failed'
  puts "# #{e.class}: #{e.message}"
end

#!/usr/bin/env ruby
# frozen_string_literal: true

puts '1..1'

begin
  require 'libsixel'

  Libsixel.set_encoder_threads('ALL')
  puts 'ok 1 - set_encoder_threads accepts uppercase all string input'
rescue StandardError => e
  puts 'not ok 1 - set_encoder_threads uppercase all string acceptance check failed'
  puts "# #{e.class}: #{e.message}"
end

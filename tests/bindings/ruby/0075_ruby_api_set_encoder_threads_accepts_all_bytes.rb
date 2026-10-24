#!/usr/bin/env ruby
# frozen_string_literal: true

puts '1..1'

begin
  require 'libsixel'

  Libsixel.set_encoder_threads('all'.b)
  puts 'ok 1 - set_encoder_threads accepts all keyword as bytes input'
rescue StandardError => e
  puts 'not ok 1 - set_encoder_threads all-byte acceptance check failed'
  puts "# #{e.class}: #{e.message}"
end

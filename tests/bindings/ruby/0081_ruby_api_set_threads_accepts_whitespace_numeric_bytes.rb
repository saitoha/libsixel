#!/usr/bin/env ruby
# frozen_string_literal: true

puts '1..1'

begin
  require 'libsixel'

  Libsixel.set_threads(' 2 '.b)
  puts 'ok 1 - set_threads accepts whitespace-padded numeric bytes input'
rescue StandardError => e
  puts 'not ok 1 - set_threads whitespace-padded numeric bytes acceptance check failed'
  puts "# #{e.class}: #{e.message}"
end

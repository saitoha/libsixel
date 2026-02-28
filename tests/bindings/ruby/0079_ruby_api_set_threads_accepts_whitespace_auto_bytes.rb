#!/usr/bin/env ruby
# frozen_string_literal: true

puts '1..1'

begin
  require 'libsixel'

  Libsixel.set_threads('  auto  '.b)
  puts 'ok 1 - set_threads accepts whitespace-padded auto bytes input'
rescue StandardError => e
  puts 'not ok 1 - set_threads whitespace-padded auto bytes acceptance check failed'
  puts "# #{e.class}: #{e.message}"
end

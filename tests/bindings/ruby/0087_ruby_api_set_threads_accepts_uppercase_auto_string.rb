#!/usr/bin/env ruby
# frozen_string_literal: true

puts '1..1'

begin
  require 'libsixel'

  Libsixel.set_threads('AUTO')
  puts 'ok 1 - set_threads accepts uppercase auto string input'
rescue StandardError => e
  puts 'not ok 1 - set_threads uppercase auto string acceptance check failed'
  puts "# #{e.class}: #{e.message}"
end

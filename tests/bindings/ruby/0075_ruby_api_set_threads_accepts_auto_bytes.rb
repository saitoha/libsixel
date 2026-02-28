#!/usr/bin/env ruby
# frozen_string_literal: true

puts '1..1'

begin
  require 'libsixel'

  Libsixel.set_threads('auto'.b)
  puts 'ok 1 - set_threads accepts auto keyword as bytes input'
rescue StandardError => e
  puts 'not ok 1 - set_threads auto-byte acceptance check failed'
  puts "# #{e.class}: #{e.message}"
end

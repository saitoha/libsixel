#!/usr/bin/env ruby
# frozen_string_literal: true

puts '1..1'

begin
  require 'libsixel'

  begin
    Libsixel.set_threads("\xFF".b)
    puts 'not ok 1 - set_threads accepted non-UTF-8 bytes input'
  rescue ArgumentError
    puts 'ok 1 - set_threads rejects non-UTF-8 bytes input'
  end
rescue StandardError => e
  puts 'not ok 1 - set_threads non-UTF-8 bytes rejection check failed'
  puts "# #{e.class}: #{e.message}"
end

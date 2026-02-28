#!/usr/bin/env ruby
# frozen_string_literal: true

puts '1..1'

begin
  require 'libsixel'

  output = Output.new(write_proc: lambda { |_chunk, _priv| 0 })
  begin
    output.set_skip_header
    puts 'not ok 1 - output setter accepted missing argument'
  rescue ArgumentError
    puts 'ok 1 - output setter rejects missing argument'
  end
rescue StandardError => e
  puts 'not ok 1 - output setter missing-argument check failed'
  puts "# #{e.class}: #{e.message}"
end

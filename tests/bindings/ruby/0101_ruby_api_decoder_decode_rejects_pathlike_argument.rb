#!/usr/bin/env ruby
# frozen_string_literal: true

puts '1..1'

begin
  require 'libsixel'

  decoder = Decoder.new
  pathlike = Class.new do
    def to_path
      File.expand_path('${TOP_SRCDIR}/tests/data/inputs/snake_64.six')
    end
  end.new

  begin
    decoder.decode(pathlike)
    puts 'not ok 1 - decoder decode accepted unexpected pathlike argument'
  rescue ArgumentError
    puts 'ok 1 - decoder decode rejects unexpected pathlike argument'
  end
rescue StandardError => e
  puts 'not ok 1 - decoder decode pathlike rejection check failed'
  puts "# #{e.class}: #{e.message}"
end

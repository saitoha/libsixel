#!/usr/bin/env ruby
# frozen_string_literal: true

puts '1..1'

begin
  require 'libsixel'

  encoder = Encoder.new
  begin
    encoder.setopt(Libsixel::API::SIXEL_OPTFLAG_COLORS.ord, '16')
    puts 'ok 1 - encoder setopt accepts integer option flag'
  rescue RuntimeError, TypeError, ArgumentError => e
    puts 'not ok 1 - encoder setopt rejected integer option flag'
    puts "# #{e.class}: #{e.message}"
  end
rescue StandardError => e
  puts 'not ok 1 - encoder integer-flag setopt check failed'
  puts "# #{e.class}: #{e.message}"
end

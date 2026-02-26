#!/usr/bin/env ruby
# frozen_string_literal: true

puts '1..1'

begin
  require 'libsixel'

  required_constants = [
    Libsixel,
    Libsixel::API,
    Libsixel::Helper,
    Encoder,
    Decoder,
    Dither,
    Output
  ]

  if required_constants.all?
    puts 'ok 1 - ruby binding exports expected modules and classes'
  else
    puts 'not ok 1 - ruby binding is missing expected exports'
  end
rescue StandardError => e
  puts 'not ok 1 - ruby module export check failed'
  puts "# #{e.class}: #{e.message}"
end

#!/usr/bin/env ruby
# frozen_string_literal: true

puts '1..1'

begin
  require 'libsixel'

  output = Output.new(write_proc: ->(_data, _priv) { nil })
  output.set_8bit_availability(1)
  output.set_gri_arg_limit(1)
  output.set_penetrate_multiplexer(1)
  output.set_skip_dcs_envelope(1)
  output.set_skip_header(1)
  output.set_palette_type(Libsixel::API::SIXEL_PALETTETYPE_RGB)
  output.set_ormode(1)
  output.set_encode_policy(Libsixel::API::SIXEL_ENCODEPOLICY_FAST)

  puts 'ok 1 - output setter APIs accept expected argument values'
rescue StandardError => e
  puts 'not ok 1 - output setter API check failed'
  puts "# #{e.class}: #{e.message}"
end

#!/usr/bin/env ruby
# frozen_string_literal: true

require 'fileutils'

puts '1..1'
artifact_local_dir = ENV.fetch('ARTIFACT_LOCAL_DIR')
FileUtils.mkdir_p(artifact_local_dir)

begin
  require 'libsixel'

  source = File.join(ENV.fetch('TOP_SRCDIR'), 'tests/data/inputs/snake_64.png')
  output = File.join(artifact_local_dir, 'ruby_bindings_smoke.six')

  encoder = Encoder.new
  encoder.setopt(Libsixel::API::SIXEL_OPTFLAG_OUTPUT, output)
  encoder.setopt(Libsixel::API::SIXEL_OPTFLAG_COLORS, '16')
  encoder.encode(source)

  payload = File.binread(output)
  if payload.empty?
    raise RuntimeError, 'encoded output is empty'
  end
  unless payload.start_with?("\ePq")
    raise RuntimeError, 'missing sixel DCS introducer'
  end
  unless payload.rstrip.end_with?("\e\\")
    raise RuntimeError, 'missing sixel ST terminator'
  end

  puts 'ok 1 - encode output generated from packaged ruby binding'
rescue StandardError => e
  puts 'not ok 1 - ruby packaged encode output check failed'
  puts "# #{e.class}: #{e.message}"
end

#!/usr/bin/env ruby
# frozen_string_literal: true

require 'fileutils'

puts '1..1'
artifact_local_dir = ENV.fetch('ARTIFACT_LOCAL_DIR')
FileUtils.mkdir_p(artifact_local_dir)


begin
  require 'libsixel'

  artifact_dir = ENV.fetch('ARTIFACT_LOCAL_DIR')
  output = File.join(artifact_dir, 'decoder_setopt_int_flag_output.png')

  decoder = Decoder.new
  begin
    decoder.setopt(Libsixel::API::SIXEL_OPTFLAG_OUTPUT, output)
    puts 'ok 1 - decoder accepts integer option flag via wrapper'
  rescue RuntimeError
    puts 'not ok 1 - decoder rejected integer option flag via wrapper'
  end
rescue StandardError => e
  puts 'not ok 1 - decoder integer option flag acceptance check failed'
  puts "# #{e.class}: #{e.message}"
end

#!/usr/bin/env ruby
# frozen_string_literal: true

require 'fiddle'

puts '1..1'

begin
  require 'libsixel'

  out = Libsixel::API::Util.make_outptr
  status = Libsixel::API.sixel_loader_new(out, 0)
  raise RuntimeError, "sixel_loader_new failed: #{status}" if Libsixel::API.failed?(status)

  loader = Libsixel::API::Util.read_outptr(out)
  begin
    status = Libsixel::API.sixel_loader_setopt(
      loader,
      Libsixel::API::SIXEL_LOADER_OPTION_CONTEXT,
      Fiddle::Pointer.new(0)
    )

    if Libsixel::API.failed?(status)
      puts 'ok 1 - loader setopt accepts Fiddle::Pointer value type'
    else
      puts 'ok 1 - loader setopt accepts Fiddle::Pointer value type'
    end
  rescue TypeError => e
    puts 'not ok 1 - loader setopt rejected Fiddle::Pointer value type'
    puts "# #{e.class}: #{e.message}"
  ensure
    Libsixel::API.sixel_loader_unref(loader) if loader && loader.to_i != 0
  end
rescue StandardError => e
  puts 'not ok 1 - loader setopt pointer value acceptance check failed'
  puts "# #{e.class}: #{e.message}"
end

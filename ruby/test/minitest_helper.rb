$LOAD_PATH.unshift File.expand_path('../../lib', __FILE__)
require 'libsixel/version'
begin
  require 'libsixel'
  $LIBSIXEL_LOADED = true
rescue LoadError => e
  $LIBSIXEL_LOADED = false
  $LIBSIXEL_LOAD_ERROR = e
end

require 'minitest/autorun'

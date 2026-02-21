require 'minitest_helper'

class TestLibsixelVersion < Minitest::Test
  def test_that_it_has_a_version_number
    refute_nil ::Libsixel::VERSION
  end
end

class TestLibsixel < Minitest::Test
  def setup
    skip("libsixel shared library not found: #{$LIBSIXEL_LOAD_ERROR}") unless $LIBSIXEL_LOADED
    @encoder = Encoder.new
  end

  def test_throws_runtime_error_when_invalid_option_detected
    assert_raises(RuntimeError) { @encoder.setopt('X', '16') }
  end
end


class TestLibsixelOptflagSync < Minitest::Test
  def test_optflag_constants_follow_public_header
    skip("libsixel shared library not found: #{$LIBSIXEL_LOAD_ERROR}") unless $LIBSIXEL_LOADED

    header = File.expand_path('../../include/sixel.h.in', __dir__)
    text = File.read(header, encoding: 'UTF-8')
    names = text.scan(/#define\s+(SIXEL_OPTFLAG_[A-Z0-9_]+)\s+\(/).flatten
    missing = names.reject { |name| Libsixel::API.const_defined?(name.to_sym) }

    assert_equal([], missing, 'missing Ruby optflag constants')
  end
end

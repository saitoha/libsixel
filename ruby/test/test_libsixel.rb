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

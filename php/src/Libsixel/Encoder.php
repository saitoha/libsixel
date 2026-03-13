<?php
/**
 * libsixel high-level encoder wrapper.
 *
 * @license MIT
 */

declare(strict_types=1);

namespace Libsixel;

final class Encoder
{
    /** @var \FFI\CData|null */
    private $encoder;

    public function __construct()
    {
        $this->encoder = API::encoderNew();
    }

    public function __destruct()
    {
        $this->close();
    }

    public function close(): void
    {
        if ($this->encoder === null) {
            return;
        }

        API::encoderUnref($this->encoder);
        $this->encoder = null;
    }

    public function setopt($flag, ?string $arg = null): void
    {
        API::encoderSetopt($this->requireEncoder(), self::normalizeFlag($flag), $arg);
    }

    public function encode(string $filename = '-'): void
    {
        API::encoderEncode($this->requireEncoder(), $filename);
    }

    private function requireEncoder()
    {
        if ($this->encoder === null) {
            throw new Exception('encoder has been closed');
        }
        return $this->encoder;
    }

    private static function normalizeFlag($flag): int
    {
        if (is_int($flag)) {
            if ($flag < 0 || $flag > 0xff) {
                throw new \InvalidArgumentException(
                    'flag integer must be in range 0..255'
                );
            }
            return $flag;
        }

        if (!is_string($flag)) {
            throw new \InvalidArgumentException(
                'flag must be a one-byte string or an integer'
            );
        }

        if (strlen($flag) !== 1) {
            throw new \InvalidArgumentException('flag string must be exactly 1 byte');
        }

        return ord($flag);
    }
}

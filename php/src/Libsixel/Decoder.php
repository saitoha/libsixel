<?php
/**
 * libsixel high-level decoder wrapper.
 *
 * @license MIT
 */

declare(strict_types=1);

namespace Libsixel;

final class Decoder
{
    /** @var \FFI\CData|null */
    private $decoder;

    public function __construct()
    {
        $this->decoder = API::decoderNew();
    }

    public function __destruct()
    {
        $this->close();
    }

    public function close(): void
    {
        if ($this->decoder === null) {
            return;
        }

        API::decoderUnref($this->decoder);
        $this->decoder = null;
    }

    public function setopt($flag, ?string $arg = null): void
    {
        API::decoderSetopt($this->requireDecoder(), self::normalizeFlag($flag), $arg);
    }

    public function decode(?string $filename = null): void
    {
        API::decoderDecode($this->requireDecoder(), $filename);
    }

    private function requireDecoder()
    {
        if ($this->decoder === null) {
            throw new Exception('decoder has been closed');
        }
        return $this->decoder;
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

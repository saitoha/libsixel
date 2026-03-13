<?php
/**
 * libsixel FFI bridge.
 *
 * @license MIT
 */

declare(strict_types=1);

namespace Libsixel;

final class API
{
    private const CDEF = <<<'CDEF'
typedef int SIXELSTATUS;
typedef struct sixel_encoder sixel_encoder_t;
typedef struct sixel_decoder sixel_decoder_t;

SIXELSTATUS sixel_encoder_new(sixel_encoder_t **ppencoder, void *allocator);
void sixel_encoder_unref(sixel_encoder_t *encoder);
SIXELSTATUS sixel_encoder_setopt(sixel_encoder_t *encoder, int arg, const char *value);
SIXELSTATUS sixel_encoder_encode(sixel_encoder_t *encoder, const char *filename);

SIXELSTATUS sixel_decoder_new(sixel_decoder_t **ppdecoder, void *allocator);
void sixel_decoder_unref(sixel_decoder_t *decoder);
SIXELSTATUS sixel_decoder_setopt(sixel_decoder_t *decoder, int arg, const char *value);
SIXELSTATUS sixel_decoder_decode(sixel_decoder_t *decoder);

const char *sixel_helper_format_error(SIXELSTATUS status);
void sixel_set_threads(int nthreads);
CDEF;

    private const LIB_NAMES = [
        'sixel',
        'libsixel',
        'sixel-1',
        'libsixel-1',
        'msys-sixel',
        'cygsixel',
        'libsixel.dylib',
    ];

    /** @var \FFI|null */
    private static $ffi;

    private function __construct()
    {
    }

    /**
     * @return \FFI
     */
    public static function ffi()
    {
        if (self::$ffi !== null) {
            return self::$ffi;
        }

        if (!extension_loaded('FFI')) {
            throw new Exception(
                'PHP FFI extension is not available. Install/enable ext-ffi first.'
            );
        }

        $errors = [];
        foreach (self::libraryCandidates() as $candidate) {
            try {
                self::$ffi = \FFI::cdef(self::CDEF, $candidate);
                return self::$ffi;
            } catch (\Throwable $e) {
                $errors[] = $candidate . ': ' . $e->getMessage();
            }
        }

        throw new Exception(
            "libsixel shared library not found. Set LIBSIXEL_LIBDIR to the "
            . "directory containing libsixel.\nTried:\n - "
            . implode("\n - ", $errors)
        );
    }

    public static function succeeded(int $status): bool
    {
        return (($status & Constants::SIXEL_FALSE) === 0);
    }

    public static function failed(int $status): bool
    {
        return (($status & Constants::SIXEL_FALSE) !== 0);
    }

    public static function setThreads($value): void
    {
        if (is_string($value)) {
            $text = trim($value);
            if (strcasecmp($text, 'auto') === 0) {
                $count = 0;
            } else {
                if (!preg_match('/^[0-9]+$/', $text)) {
                    throw new \InvalidArgumentException(
                        "threads must be a positive integer or 'auto'"
                    );
                }
                $count = (int)$text;
            }
        } elseif (is_int($value)) {
            $count = $value;
        } else {
            throw new \InvalidArgumentException(
                "threads must be a positive integer or 'auto'"
            );
        }

        if ($count < 0) {
            throw new \InvalidArgumentException(
                "threads must be a positive integer or 'auto'"
            );
        }

        self::ffi()->sixel_set_threads($count);
    }

    public static function encoderNew()
    {
        $ffi = self::ffi();
        $out = $ffi->new('sixel_encoder_t *[1]');
        $status = (int)$ffi->sixel_encoder_new($out, null);
        self::throwOnError($status, 'sixel_encoder_new');
        return $out[0];
    }

    public static function encoderUnref($encoder): void
    {
        if ($encoder !== null) {
            self::ffi()->sixel_encoder_unref($encoder);
        }
    }

    public static function encoderSetopt($encoder, int $flag, ?string $arg): void
    {
        $status = (int)self::ffi()->sixel_encoder_setopt($encoder, $flag, $arg);
        self::throwOnError($status, 'sixel_encoder_setopt');
    }

    public static function encoderEncode($encoder, string $filename): void
    {
        $status = (int)self::ffi()->sixel_encoder_encode($encoder, $filename);
        self::throwOnError($status, 'sixel_encoder_encode');
    }

    public static function decoderNew()
    {
        $ffi = self::ffi();
        $out = $ffi->new('sixel_decoder_t *[1]');
        $status = (int)$ffi->sixel_decoder_new($out, null);
        self::throwOnError($status, 'sixel_decoder_new');
        return $out[0];
    }

    public static function decoderUnref($decoder): void
    {
        if ($decoder !== null) {
            self::ffi()->sixel_decoder_unref($decoder);
        }
    }

    public static function decoderSetopt($decoder, int $flag, ?string $arg): void
    {
        $status = (int)self::ffi()->sixel_decoder_setopt($decoder, $flag, $arg);
        self::throwOnError($status, 'sixel_decoder_setopt');
    }

    public static function decoderDecode($decoder, ?string $filename): void
    {
        if ($filename !== null) {
            self::decoderSetopt($decoder, ord(Constants::SIXEL_OPTFLAG_INPUT), $filename);
        }
        $status = (int)self::ffi()->sixel_decoder_decode($decoder);
        self::throwOnError($status, 'sixel_decoder_decode');
    }

    public static function formatError(int $status): string
    {
        $raw = self::ffi()->sixel_helper_format_error($status);
        if ($raw === null) {
            return sprintf('libsixel error 0x%04x', $status);
        }
        return \FFI::string($raw);
    }

    public static function throwOnError(int $status, string $context): void
    {
        if (!self::failed($status)) {
            return;
        }
        $message = self::formatError($status);
        throw new Exception($context . ': ' . $message, $status);
    }

    /**
     * @return list<string>
     */
    private static function libraryCandidates(): array
    {
        $candidates = [];

        $libpath = getenv('LIBSIXEL_LIBPATH');
        if (is_string($libpath) && $libpath !== '') {
            $candidates[] = $libpath;
        }

        $bundledDir = dirname(__DIR__) . '/_libs';
        $candidates = array_merge($candidates, self::findLibrariesInDir($bundledDir));

        $envLibDir = getenv('LIBSIXEL_LIBDIR');
        if (is_string($envLibDir) && $envLibDir !== '') {
            $candidates = array_merge($candidates, self::findLibrariesInDir($envLibDir));
        }

        foreach (self::LIB_NAMES as $name) {
            $candidates[] = $name;
        }

        $seen = [];
        $unique = [];
        foreach ($candidates as $candidate) {
            if (!is_string($candidate) || $candidate === '') {
                continue;
            }
            if (isset($seen[$candidate])) {
                continue;
            }
            $seen[$candidate] = true;
            $unique[] = $candidate;
        }

        return $unique;
    }

    /**
     * @return list<string>
     */
    private static function findLibrariesInDir(string $libDir): array
    {
        if (!is_dir($libDir)) {
            return [];
        }

        $prefixes = ['lib', ''];
        $matches = [];
        foreach (self::LIB_NAMES as $name) {
            foreach ($prefixes as $prefix) {
                $base = $prefix . $name;

                $patterns = [
                    $libDir . DIRECTORY_SEPARATOR . $base . '*.dylib',
                    $libDir . DIRECTORY_SEPARATOR . $base . '*.dll',
                    $libDir . DIRECTORY_SEPARATOR . $base . '*.so',
                    $libDir . DIRECTORY_SEPARATOR . $base . '*.so.*',
                ];

                foreach ($patterns as $pattern) {
                    $items = glob($pattern);
                    if (!is_array($items)) {
                        continue;
                    }
                    foreach ($items as $item) {
                        if (!is_file($item)) {
                            continue;
                        }
                        if (substr($item, -6) === '.dll.a') {
                            continue;
                        }
                        if (substr($item, -8) === '.dll.def') {
                            continue;
                        }
                        if (substr($item, -4) === '.lib') {
                            continue;
                        }
                        $matches[] = $item;
                    }
                }
            }
        }

        sort($matches);
        return array_values(array_unique($matches));
    }
}

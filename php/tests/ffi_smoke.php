<?php
/**
 * Minimal runtime smoke test for the PHP FFI bindings.
 */

declare(strict_types=1);

require_once dirname(__DIR__) . '/src/autoload.php';

use Libsixel\Constants;
use Libsixel\Encoder;

echo "1..3\n";

try {
    $libdir = getenv('LIBSIXEL_LIBDIR');
    if (!is_string($libdir) || $libdir === '') {
        putenv('LIBSIXEL_LIBDIR=' . dirname(__DIR__, 2) . '/src/.libs');
    }
    echo "ok 1 - configured library search path\n";

    $encoder = new Encoder();
    echo "ok 2 - encoder constructed\n";

    $outfile = sys_get_temp_dir() . '/libsixel-php-smoke-' . getmypid() . '.six';
    $encoder->setopt(Constants::SIXEL_OPTFLAG_OUTPUT, $outfile);
    $encoder->setopt(Constants::SIXEL_OPTFLAG_COLORS, '16');
    $encoder->encode(dirname(__DIR__, 2) . '/images/egret.jpg');
    $encoder->close();

    if (!is_file($outfile) || filesize($outfile) === 0) {
        throw new RuntimeException('sixel output was not generated');
    }
    @unlink($outfile);
    echo "ok 3 - encode output generated\n";
} catch (Throwable $e) {
    $message = preg_replace('/\s+/', ' ', $e->getMessage());
    if ($message === null || $message === '') {
        $message = 'unknown error';
    }
    echo "not ok 3 - {$message}\n";
    exit(1);
}

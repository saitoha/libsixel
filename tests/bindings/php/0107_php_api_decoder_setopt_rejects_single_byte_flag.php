#!/usr/bin/env php
<?php

declare(strict_types=1);

use Libsixel\API;

echo "1..1\n";

$bindingRoot = (string) getenv('SIXEL_TEST_PHP_BINDING_ROOT');

require_once $bindingRoot . '/src/autoload.php';

$decoder = null;

try {
    $decoder = API::decoderNew();

    try {
        API::decoderSetopt($decoder, 'i', 'dummy.png');
        echo "not ok 1 - raw decoder_setopt accepted single-byte string option flag input\n";
    } catch (TypeError $e) {
        echo "ok 1 - raw decoder_setopt rejects single-byte string option flag input\n";
    }
} catch (Throwable $e) {
    echo "not ok 1 - raw decoder_setopt single-byte flag rejection check failed\n";
    echo '# ' . get_class($e) . ': ' . preg_replace('/\\s+/', ' ', $e->getMessage()) . "\n";
} finally {
    if ($decoder !== null) {
        API::decoderUnref($decoder);
    }
}

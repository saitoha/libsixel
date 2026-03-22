#!/usr/bin/env php
<?php

declare(strict_types=1);

use Libsixel\API;

echo "1..1\n";

$bindingRoot = (string) getenv('SIXEL_TEST_PHP_BINDING_ROOT');

require_once $bindingRoot . '/src/autoload.php';

$encoder = null;

try {
    $encoder = API::encoderNew();

    $multi = pack('C*', ord('p'), ord('p'));
    try {
        API::encoderSetopt($encoder, $multi, '256');
        echo "not ok 1 - raw encoder_setopt accepted multi-byte bytes option flag input\n";
    } catch (TypeError $e) {
        echo "ok 1 - raw encoder_setopt rejects multi-byte bytes option flag input\n";
    }
} catch (Throwable $e) {
    echo "not ok 1 - raw encoder_setopt multi-byte bytes flag rejection check failed\n";
    echo '# ' . get_class($e) . ': ' . preg_replace('/\\s+/', ' ', $e->getMessage()) . "\n";
} finally {
    if ($encoder !== null) {
        API::encoderUnref($encoder);
    }
}

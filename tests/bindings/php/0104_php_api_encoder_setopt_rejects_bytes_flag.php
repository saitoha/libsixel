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

    API::encoderSetopt($encoder, ord('p'), '16');

    $bytesFlag = pack('C*', ord('p'));

    try {
        API::encoderSetopt($encoder, $bytesFlag, '16');
        echo "not ok 1 - raw encoder_setopt accepted bytes option flag input\n";
    } catch (TypeError $e) {
        echo "ok 1 - raw encoder_setopt rejects bytes option flag input\n";
    }
} catch (Throwable $e) {
    echo "not ok 1 - raw encoder_setopt bytes option flag rejection check failed\n";
    echo '# ' . get_class($e) . ': ' . preg_replace('/\\s+/', ' ', $e->getMessage()) . "\n";
} finally {
    if ($encoder !== null) {
        API::encoderUnref($encoder);
    }
}

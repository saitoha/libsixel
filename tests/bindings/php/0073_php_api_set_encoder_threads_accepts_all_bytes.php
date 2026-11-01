#!/usr/bin/env php
<?php

declare(strict_types=1);

use Libsixel\API;

echo "1..1\n";

$bindingRoot = (string) getenv('SIXEL_TEST_PHP_BINDING_ROOT');

require_once $bindingRoot . '/src/autoload.php';

try {
    API::setEncoderThreads('all');

    echo "ok 1 - set_encoder_threads accepts all keyword as bytes-like input\n";
} catch (Throwable $e) {
    echo "not ok 1 - set_encoder_threads all-byte acceptance check failed\n";
    echo '# ' . get_class($e) . ': ' . preg_replace('/\\s+/', ' ', $e->getMessage()) . "\n";
}

#!/usr/bin/env php
<?php

declare(strict_types=1);

use Libsixel\API;

echo "1..1\n";

$bindingRoot = (string) getenv('SIXEL_TEST_PHP_BINDING_ROOT');


require_once $bindingRoot . '/src/autoload.php';

try {
    $missing = [];

    foreach ([
        'Libsixel\\API',
        'Libsixel\\Constants',
        'Libsixel\\Encoder',
        'Libsixel\\Decoder',
        'Libsixel\\Exception',
        'SixelEncoder',
        'SixelDecoder',
    ] as $class) {
        if (!class_exists($class)) {
            $missing[] = $class;
        }
    }

    foreach ([
        'succeeded',
        'failed',
        'setThreads',
        'formatError',
        'encoderNew',
        'encoderSetopt',
        'encoderEncode',
        'encoderUnref',
        'decoderNew',
        'decoderSetopt',
        'decoderDecode',
        'decoderUnref',
    ] as $method) {
        if (!method_exists(API::class, $method)) {
            $missing[] = 'Libsixel\\API::' . $method;
        }
    }

    if ($missing === []) {
        echo "ok 1 - php binding exports expected classes and API methods\n";
    } else {
        echo "not ok 1 - php binding is missing expected exports: " . implode(', ', $missing) . "\n";
    }
} catch (Throwable $e) {
    echo "not ok 1 - php module export check failed\n";
    echo '# ' . get_class($e) . ': ' . preg_replace('/\\s+/', ' ', $e->getMessage()) . "\n";
}

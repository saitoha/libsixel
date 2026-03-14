#!/usr/bin/env php
<?php

declare(strict_types=1);

use Libsixel\Constants;
use Libsixel\Decoder;

echo "1..1\n";

$bindingRoot = (string) getenv('SIXEL_TEST_PHP_BINDING_ROOT');
$devNull = DIRECTORY_SEPARATOR === '\\' ? 'NUL' : '/dev/null';


require_once $bindingRoot . '/src/autoload.php';

try {
    $decoder = new Decoder();
    $decoder->setopt(Constants::SIXEL_OPTFLAG_OUTPUT, $devNull);

    echo "ok 1 - decoder setopt is callable after initialize\n";
} catch (Throwable $e) {
    echo "not ok 1 - decoder post-initialize setopt check failed\n";
    echo '# ' . get_class($e) . ': ' . preg_replace('/\s+/', ' ', $e->getMessage()) . "\n";
}

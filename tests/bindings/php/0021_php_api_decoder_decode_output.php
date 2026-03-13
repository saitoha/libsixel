#!/usr/bin/env php
<?php

declare(strict_types=1);

use Libsixel\Constants;
use Libsixel\Decoder;

echo "1..1\n";

$bindingRoot = getenv('SIXEL_TEST_PHP_BINDING_ROOT');
$sourceRoot = getenv('TOP_SRCDIR');
$devNull = DIRECTORY_SEPARATOR === '\\' ? 'NUL' : '/dev/null';

if (!is_string($bindingRoot) || $bindingRoot === '' || !is_string($sourceRoot) || $sourceRoot === '') {
    echo "not ok 1 - required test environment variables are missing\n";
    exit(1);
}

require_once $bindingRoot . '/src/autoload.php';

try {
    $source = $sourceRoot . '/tests/data/inputs/snake_64.six';

    $decoder = new Decoder();
    $decoder->setopt(Constants::SIXEL_OPTFLAG_INPUT, $source);
    $decoder->setopt(Constants::SIXEL_OPTFLAG_OUTPUT, $devNull);
    $decoder->decode();

    echo "ok 1 - decoder decode accepts input and configured output\n";
} catch (Throwable $e) {
    echo "not ok 1 - decoder decode output check failed\n";
    echo '# ' . get_class($e) . ': ' . preg_replace('/\s+/', ' ', $e->getMessage()) . "\n";
}

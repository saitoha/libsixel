#!/usr/bin/env php
<?php

declare(strict_types=1);

use Libsixel\Constants;
use Libsixel\Encoder;

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
    $source = $sourceRoot . '/tests/data/inputs/formats/apng_invalid_num_frames_zero.png';

    $encoder = new Encoder();
    $encoder->setopt(Constants::SIXEL_OPTFLAG_OUTPUT, $devNull);

    try {
        $encoder->encode($source);
        echo "ok 1 - encoder handles corrupted-style input without crashing\n";
    } catch (Throwable $e) {
        echo "ok 1 - encoder rejects corrupted-style input\n";
    }
} catch (Throwable $e) {
    echo "not ok 1 - corrupted image handling check failed\n";
    echo '# ' . get_class($e) . ': ' . preg_replace('/\s+/', ' ', $e->getMessage()) . "\n";
}

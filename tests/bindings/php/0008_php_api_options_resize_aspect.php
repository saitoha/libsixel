#!/usr/bin/env php
<?php

declare(strict_types=1);

use Libsixel\Constants;
use Libsixel\Encoder;

echo "1..1\n";

$bindingRoot = (string) getenv('SIXEL_TEST_PHP_BINDING_ROOT');
$sourceRoot = (string) getenv('TOP_SRCDIR');
$devNull = DIRECTORY_SEPARATOR === '\\' ? 'NUL' : '/dev/null';


require_once $bindingRoot . '/src/autoload.php';

try {
    $source = $sourceRoot . '/tests/data/inputs/snake_64.png';

    $encoder = new Encoder();
    $encoder->setopt(Constants::SIXEL_OPTFLAG_OUTPUT, $devNull);
    $encoder->setopt(Constants::SIXEL_OPTFLAG_WIDTH, '96');
    $encoder->setopt(Constants::SIXEL_OPTFLAG_HEIGHT, 'auto');
    $encoder->encode($source);

    echo "ok 1 - aspect resize options produce valid encode path\n";
} catch (Throwable $e) {
    echo "not ok 1 - aspect resize option check failed\n";
    echo '# ' . get_class($e) . ': ' . preg_replace('/\s+/', ' ', $e->getMessage()) . "\n";
}

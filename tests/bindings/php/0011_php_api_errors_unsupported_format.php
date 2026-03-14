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
    $source = $sourceRoot . '/README.md';

    $encoder = new Encoder();
    $encoder->setopt(Constants::SIXEL_OPTFLAG_OUTPUT, $devNull);
    $encoder->setopt(Constants::SIXEL_OPTFLAG_LOADERS, 'builtin!');
    $encoder->encode($source);
    echo "not ok 1 - encoder accepted unsupported format input\n";
} catch (Throwable $e) {
    echo "ok 1 - encoder rejects unsupported format input\n";
}

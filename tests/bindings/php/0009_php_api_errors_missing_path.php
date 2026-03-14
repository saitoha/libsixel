#!/usr/bin/env php
<?php

declare(strict_types=1);

use Libsixel\Constants;
use Libsixel\Encoder;

echo "1..1\n";

$bindingRoot = (string) getenv('SIXEL_TEST_PHP_BINDING_ROOT');
$devNull = DIRECTORY_SEPARATOR === '\\' ? 'NUL' : '/dev/null';


require_once $bindingRoot . '/src/autoload.php';

try {
    $encoder = new Encoder();
    $encoder->setopt(Constants::SIXEL_OPTFLAG_OUTPUT, $devNull);
    $encoder->encode('/path/that/does/not/exist.png');
    echo "not ok 1 - encoder accepted missing input path\n";
} catch (Throwable $e) {
    echo "ok 1 - encoder rejects missing input path\n";
}

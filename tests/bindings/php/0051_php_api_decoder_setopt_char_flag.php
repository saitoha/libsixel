#!/usr/bin/env php
<?php

declare(strict_types=1);

use Libsixel\Constants;
use Libsixel\Decoder;

echo "1..1\n";

$bindingRoot = (string) getenv('SIXEL_TEST_PHP_BINDING_ROOT');

require_once $bindingRoot . '/src/autoload.php';

$decoder = null;

try {
    $decoder = new Decoder();
    $decoder->setopt(Constants::SIXEL_OPTFLAG_OUTPUT, 'dummy.png');

    echo "ok 1 - decoder setopt accepts character option flag\n";
} catch (Throwable $e) {
    echo "not ok 1 - decoder character-flag setopt check failed\n";
    echo '# ' . get_class($e) . ': ' . preg_replace('/\\s+/', ' ', $e->getMessage()) . "\n";
} finally {
    if ($decoder !== null) {
        $decoder->close();
    }
}

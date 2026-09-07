#!/usr/bin/env php
<?php

declare(strict_types=1);

use Libsixel\Constants;
use Libsixel\Encoder;

echo "1..1\n";

$bindingRoot = (string) getenv('SIXEL_TEST_PHP_BINDING_ROOT');

require_once $bindingRoot . '/src/autoload.php';

$encoder = null;

try {
    $encoder = new Encoder();
    $encoder->setopt(
        Constants::SIXEL_OPTFLAG_SAMPLING_POLICY,
        'adaptive-grid'
    );
    $encoder->setopt(
        Constants::SIXEL_OPTFLAG_BINNING_POLICY,
        'hard'
    );
    $encoder->setopt(
        Constants::SIXEL_OPTFLAG_BACKGROUND_POLICY,
        'explicit_first'
    );

    echo "ok 1 - encoder accepts a numeric long-only option flag\n";
} catch (Throwable $e) {
    echo "not ok 1 - encoder long-only option flag check failed\n";
    echo '# ' . get_class($e) . ': ' . preg_replace('/\\s+/', ' ', $e->getMessage()) . "\n";
} finally {
    if ($encoder !== null) {
        $encoder->close();
    }
}

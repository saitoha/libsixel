#!/usr/bin/env php
<?php

declare(strict_types=1);

use Libsixel\Constants;
use Libsixel\Encoder;

echo "1..1\n";

$bindingRoot = (string) getenv('SIXEL_TEST_PHP_BINDING_ROOT');
$artifactLocalDir = (string) getenv('ARTIFACT_LOCAL_DIR');
$sourceRoot = (string) getenv('TOP_SRCDIR');


if (!is_string($artifactLocalDir) || $artifactLocalDir === '') {
    $artifactLocalDir = sys_get_temp_dir() . '/libsixel-php-artifacts';
}
if (!is_dir($artifactLocalDir) && !mkdir($artifactLocalDir, 0777, true) && !is_dir($artifactLocalDir)) {
    echo "not ok 1 - failed to create artifact directory\n";
    exit(1);
}

require_once $bindingRoot . '/src/autoload.php';

try {
    $source = $sourceRoot . '/tests/data/inputs/snake_64.png';
    $output = $artifactLocalDir . '/format_png.six';

    $encoder = new Encoder();
    $encoder->setopt(Constants::SIXEL_OPTFLAG_OUTPUT, $output);
    $encoder->encode($source);

    $payload = file_get_contents($output);
    if (!is_string($payload) || $payload === '') {
        throw new RuntimeException('encoded output is empty');
    }

    $trimmed = preg_replace('/[\r\n]+\z/', '', $payload);
    if (!is_string($trimmed)
        || strpos($trimmed, "\x1bPq") !== 0
        || substr($trimmed, -2) !== "\x1b\\"
    ) {
        throw new RuntimeException('encoded output does not contain a valid sixel envelope');
    }

    echo "ok 1 - encoder writes sixel envelope for png input\n";
} catch (Throwable $e) {
    echo "not ok 1 - png encode envelope check failed\n";
    echo '# ' . get_class($e) . ': ' . preg_replace('/\s+/', ' ', $e->getMessage()) . "\n";
}

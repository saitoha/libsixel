#!/usr/bin/env php
<?php

declare(strict_types=1);

use Libsixel\Constants;
use Libsixel\Encoder;

echo "1..1\n";

$bindingRoot = (string) getenv('SIXEL_TEST_PHP_BINDING_ROOT');
$resolvedBindingRoot = realpath($bindingRoot);
if (!is_string($resolvedBindingRoot)) {
    echo "not ok 1 - failed to resolve php binding root\n";
    exit(0);
}

require_once $resolvedBindingRoot . '/src/autoload.php';

try {
    $artifactLocalDir = (string) getenv('ARTIFACT_LOCAL_DIR');
    if ($artifactLocalDir === '') {
        $artifactLocalDir = sys_get_temp_dir() . '/libsixel-php-artifacts';
    }
    if (!is_dir($artifactLocalDir) && !mkdir($artifactLocalDir, 0777, true)) {
        throw new RuntimeException('failed to create artifact directory');
    }

    $sourceRoot = (string) getenv('TOP_SRCDIR');
    $source = $sourceRoot . '/tests/data/inputs/snake_64.png';
    $output = $artifactLocalDir . '/php_bindings_smoke.six';

    $encoder = new Encoder();
    $encoder->setopt(Constants::SIXEL_OPTFLAG_OUTPUT, $output);
    $encoder->setopt(Constants::SIXEL_OPTFLAG_COLORS, '16');
    $encoder->encode($source);
    $encoder->close();

    if (!is_file($output)) {
        throw new RuntimeException('sixel output was not generated');
    }

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

    echo "ok 1 - encode output generated from packaged binding\n";
} catch (Throwable $e) {
    $message = preg_replace('/\s+/', ' ', $e->getMessage());
    if (!is_string($message) || $message === '') {
        $message = 'unknown failure';
    }
    echo "not ok 1 - {$message}\n";
}

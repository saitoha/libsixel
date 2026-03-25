#!/usr/bin/env php
<?php

declare(strict_types=1);

echo "1..1\n";

$bindingRoot = (string) getenv('SIXEL_TEST_PHP_BINDING_ROOT');
$libDir = (string) getenv('LIBSIXEL_LIBDIR');
$libPath = (string) getenv('LIBSIXEL_LIBPATH');

$resolvedBindingRoot = realpath($bindingRoot);
$resolvedLibDir = realpath($libDir);
$resolvedLibPath = realpath($libPath);

if (!is_string($resolvedBindingRoot) || !is_string($resolvedLibDir) || !is_string($resolvedLibPath)) {
    echo "not ok 1 - failed to resolve packaged binding and library paths\n";
    exit(0);
}

if (strpos($resolvedLibDir, $resolvedBindingRoot . DIRECTORY_SEPARATOR) === 0
    && strpos($resolvedLibPath, $resolvedBindingRoot . DIRECTORY_SEPARATOR) === 0
) {
    echo "ok 1 - packaged php binding paths are configured\n";
} else {
    echo "not ok 1 - php binding did not resolve to packaged libsixel path\n";
}

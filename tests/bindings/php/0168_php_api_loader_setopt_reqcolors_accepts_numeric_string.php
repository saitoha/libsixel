#!/usr/bin/env php
<?php

declare(strict_types=1);

use Libsixel\API;

echo "1..1\n";

$bindingRoot = (string) getenv('SIXEL_TEST_PHP_BINDING_ROOT');

require_once $bindingRoot . '/src/autoload.php';

$loader = null;

try {
    $loader = API::loaderNew();
    API::loaderSetopt($loader, 3, '32');

    API::loaderUnref($loader);
    $loader = null;

    echo "ok 1 - loader setopt accepts numeric-string reqcolors\n";
} catch (Throwable $e) {
    echo "not ok 1 - reqcolors numeric-string acceptance check failed\n";
    echo '# ' . get_class($e) . ': ' . preg_replace('/\\s+/', ' ', $e->getMessage()) . "\n";
} finally {
    if ($loader !== null) {
        try {
            API::loaderUnref($loader);
        } catch (Throwable $e) {
        }
    }
}

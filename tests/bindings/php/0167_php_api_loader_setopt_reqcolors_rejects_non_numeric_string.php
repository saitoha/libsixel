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

    $rejected = false;
    try {
        API::loaderSetopt($loader, 3, 'abc');
    } catch (\InvalidArgumentException $e) {
        $rejected = true;
    } catch (Throwable $e) {
        $rejected = true;
    }

    API::loaderUnref($loader);
    $loader = null;

    if ($rejected) {
        echo "ok 1 - loader setopt rejects non-numeric reqcolors string\n";
    } else {
        echo "not ok 1 - loader setopt accepted non-numeric reqcolors string\n";
    }
} catch (Throwable $e) {
    echo "not ok 1 - reqcolors non-numeric rejection check failed\n";
    echo '# ' . get_class($e) . ': ' . preg_replace('/\\s+/', ' ', $e->getMessage()) . "\n";
} finally {
    if ($loader !== null) {
        try {
            API::loaderUnref($loader);
        } catch (Throwable $e) {
        }
    }
}

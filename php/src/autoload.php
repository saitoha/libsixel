<?php
/**
 * libsixel PHP bindings autoloader.
 *
 * @license MIT
 */

declare(strict_types=1);

spl_autoload_register(static function (string $class): void {
    $prefix = 'Libsixel\\';
    if (strpos($class, $prefix) !== 0) {
        return;
    }

    $relative = substr($class, strlen($prefix));
    if ($relative === false || $relative === '') {
        return;
    }

    $path = __DIR__ . '/Libsixel/' . str_replace('\\', '/', $relative) . '.php';
    if (is_file($path)) {
        require_once $path;
    }
});

require_once __DIR__ . '/compat.php';

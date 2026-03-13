#!/usr/bin/env php
<?php

declare(strict_types=1);

if ($argc !== 3) {
    fwrite(STDERR, "usage: {$argv[0]} <test-script> <coverage-json>\n");
    exit(2);
}

if (!function_exists('phpdbg_start_oplog') || !function_exists('phpdbg_end_oplog')) {
    fwrite(STDERR, "phpdbg oplog functions are unavailable\n");
    exit(2);
}

$testScript = $argv[1];
$coveragePath = $argv[2];
$exitStatus = 0;

phpdbg_start_oplog();

try {
    (static function (string $script): void {
        require $script;
    })($testScript);
} catch (Throwable $e) {
    $exitStatus = 1;
    fwrite(STDERR, get_class($e) . ': ' . $e->getMessage() . PHP_EOL);
    $trace = $e->getTraceAsString();
    if ($trace !== '') {
        fwrite(STDERR, $trace . PHP_EOL);
    }
}

$oplog = phpdbg_end_oplog();
if (!is_array($oplog)) {
    $oplog = [];
}

$coverageDir = dirname($coveragePath);
if (!is_dir($coverageDir) && !mkdir($coverageDir, 0777, true) && !is_dir($coverageDir)) {
    fwrite(STDERR, "failed to create coverage directory: {$coverageDir}\n");
    if ($exitStatus === 0) {
        $exitStatus = 1;
    }
}

$json = json_encode($oplog, JSON_PRETTY_PRINT);
if (!is_string($json) || file_put_contents($coveragePath, $json) === false) {
    fwrite(STDERR, "failed to write coverage file: {$coveragePath}\n");
    if ($exitStatus === 0) {
        $exitStatus = 1;
    }
}

exit($exitStatus);

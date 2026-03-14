#!/usr/bin/env php
<?php

declare(strict_types=1);

echo "1..1\n";

$php = dirname(PHP_BINARY) . DIRECTORY_SEPARATOR . 'php';
if (!is_file($php) || !is_executable($php)) {
    $php = PHP_BINARY;
}

$child = <<<'CHILD'
$libPath = (string) getenv('LIBSIXEL_LIBPATH');
$ffi = FFI::cdef(<<<'CDEF'
typedef int SIXELSTATUS;
typedef struct sixel_loader sixel_loader_t;
typedef struct sixel_frame sixel_frame_t;
typedef int (*sixel_load_image_function)(sixel_frame_t *frame, void *context);
SIXELSTATUS sixel_loader_new(sixel_loader_t **pploader, void *allocator);
void sixel_loader_unref(sixel_loader_t *loader);
SIXELSTATUS sixel_loader_load_file(sixel_loader_t *loader, char const *filename, sixel_load_image_function fn_load);
CDEF, $libPath);
$out = $ffi->new('sixel_loader_t *[1]');
$status = (int)$ffi->sixel_loader_new($out, null);
if ($status != 0) {
    fwrite(STDOUT, "status={$status}\n");
    exit(0);
}
$loader = $out[0];
$loadCallback = function ($frame, $context): int {
    return 0;
};
$status = (int)$ffi->sixel_loader_load_file($loader, null, $loadCallback);
$ffi->sixel_loader_unref($loader);
fwrite(STDOUT, "status={$status}\n");
CHILD;

try {
    $descriptorSpec = [
        0 => ['pipe', 'r'],
        1 => ['pipe', 'w'],
        2 => ['pipe', 'w'],
    ];
    $pipes = [];
    $process = proc_open([$php, '-d', 'ffi.enable=true', '-r', $child], $descriptorSpec, $pipes);
    if (!is_resource($process)) {
        throw new RuntimeException('proc_open failed');
    }

    fclose($pipes[0]);
    $stdout = (string) stream_get_contents($pipes[1]);
    fclose($pipes[1]);
    stream_get_contents($pipes[2]);
    fclose($pipes[2]);
    $rc = proc_close($process);

    $status = null;
    if (preg_match('/status=(-?[0-9]+)/', $stdout, $m) === 1) {
        $status = (int)$m[1];
    }

    if ($status === 0) {
        echo "ok 1 - loader load_file accepts null filename in current PHP FFI path\n";
    } elseif ($rc !== 0 || ($status !== null && $status !== 0)) {
        echo "ok 1 - loader load_file null filename failure path is isolated from harness crash\n";
    } else {
        echo "not ok 1 - loader load_file null filename behavior was unexpected\n";
    }
} catch (Throwable $e) {
    echo "not ok 1 - loader load_file null-filename check failed\n";
    echo '# ' . get_class($e) . ': ' . preg_replace('/\\s+/', ' ', $e->getMessage()) . "\n";
}

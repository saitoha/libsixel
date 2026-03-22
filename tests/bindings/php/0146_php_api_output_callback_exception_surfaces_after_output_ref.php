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
typedef struct sixel_output sixel_output_t;
typedef struct sixel_dither sixel_dither_t;
typedef int (*sixel_write_function)(char *data, int size, void *priv);
SIXELSTATUS sixel_output_new(sixel_output_t **output, sixel_write_function fn_write, void *priv, void *allocator);
void sixel_output_ref(sixel_output_t *output);
void sixel_output_unref(sixel_output_t *output);
sixel_dither_t *sixel_dither_get(int builtin_dither);
void sixel_dither_unref(sixel_dither_t *dither);
int sixel_helper_compute_depth(int pixelformat);
SIXELSTATUS sixel_encode(unsigned char *pixels, int width, int height, int depth, sixel_dither_t *dither, sixel_output_t *context);
CDEF, $libPath);
$out = $ffi->new('sixel_output_t *[1]');
$status = (int)$ffi->sixel_output_new($out, function ($data, $size, $priv): int {
    throw new RuntimeException('boom after ref');
}, null, null);
if ($status !== 0) {
    exit(2);
}
$output = $out[0];
$ffi->sixel_output_ref($output);
$dither = $ffi->sixel_dither_get(0x3);
$depth = (int)$ffi->sixel_helper_compute_depth(0x03);
$pixels = $ffi->new('unsigned char[3]');
$pixels[0] = 255;
$pixels[1] = 0;
$pixels[2] = 0;
$ffi->sixel_encode($pixels, 1, 1, $depth, $dither, $output);
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
    stream_get_contents($pipes[1]);
    fclose($pipes[1]);
    stream_get_contents($pipes[2]);
    fclose($pipes[2]);
    $rc = proc_close($process);

    if ($rc !== 0) {
        echo "ok 1 - output callback exception surfaces after output_ref\n";
    } else {
        echo "not ok 1 - output callback exception did not surface after output_ref\n";
    }
} catch (Throwable $e) {
    echo "not ok 1 - output_ref callback-exception propagation check failed\n";
    echo '# ' . get_class($e) . ': ' . preg_replace('/\\s+/', ' ', $e->getMessage()) . "\n";
}

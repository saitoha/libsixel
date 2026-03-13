#!/usr/bin/env php
<?php

declare(strict_types=1);

if ($argc < 3 || $argc > 4) {
    fwrite(STDERR, "usage: {$argv[0]} <coverage-dir> <output-lcov> [source-root]\n");
    exit(2);
}

$coverageDir = $argv[1];
$outputLcov = $argv[2];
$sourceRoot = $argc === 4 ? realpath($argv[3]) : false;
$sourcePhpDir = is_string($sourceRoot) ? $sourceRoot . '/php/src/' : null;

$records = [];
$coverageFiles = glob($coverageDir . '/*.json');
if (!is_array($coverageFiles)) {
    $coverageFiles = [];
}
sort($coverageFiles);

foreach ($coverageFiles as $coverageFile) {
    $payload = file_get_contents($coverageFile);
    if (!is_string($payload) || $payload === '') {
        continue;
    }

    $decoded = json_decode($payload, true);
    if (!is_array($decoded)) {
        continue;
    }

    foreach ($decoded as $path => $lineHits) {
        if (!is_string($path) || !is_array($lineHits)) {
            continue;
        }

        $normalized = $path;
        if (is_string($sourceRoot)) {
            $candidate = null;
            if (is_string($sourcePhpDir) && strpos($path, $sourcePhpDir) === 0 && is_file($path)) {
                $candidate = $path;
            } elseif (preg_match('{/src/(.+\\.php)$}', $path, $matches) === 1) {
                $mapped = $sourceRoot . '/php/src/' . $matches[1];
                if (is_file($mapped)) {
                    $candidate = $mapped;
                }
            }

            if ($candidate === null) {
                continue;
            }
            $normalized = $candidate;

            if (is_string($sourcePhpDir) && strpos($normalized, $sourcePhpDir) !== 0) {
                continue;
            }
        }

        if (!isset($records[$normalized])) {
            $records[$normalized] = [
                'found' => [],
                'hits' => [],
            ];
        }

        foreach ($lineHits as $line => $hitCount) {
            if (!is_numeric($line) || !is_numeric($hitCount)) {
                continue;
            }
            $lineNumber = (int)$line;
            if ($lineNumber <= 0) {
                continue;
            }

            $hits = (int)$hitCount;
            if ($hits < 0) {
                $hits = 0;
            }

            $records[$normalized]['found'][$lineNumber] = true;
            if ($hits > 0) {
                if (!isset($records[$normalized]['hits'][$lineNumber])) {
                    $records[$normalized]['hits'][$lineNumber] = 0;
                }
                $records[$normalized]['hits'][$lineNumber] += $hits;
            }
        }
    }
}

ksort($records);

$out = fopen($outputLcov, 'wb');
if ($out === false) {
    fwrite(STDERR, "failed to open {$outputLcov} for writing\n");
    exit(1);
}

foreach ($records as $file => $entry) {
    $lines = array_keys($entry['found']);
    sort($lines, SORT_NUMERIC);
    if ($lines === []) {
        continue;
    }

    $linesFound = 0;
    $linesHit = 0;

    fwrite($out, "TN:\n");
    fwrite($out, "SF:{$file}\n");

    foreach ($lines as $lineNumber) {
        $hits = $entry['hits'][$lineNumber] ?? 0;
        fwrite($out, "DA:{$lineNumber},{$hits}\n");
        $linesFound++;
        if ($hits > 0) {
            $linesHit++;
        }
    }

    fwrite($out, "LF:{$linesFound}\n");
    fwrite($out, "LH:{$linesHit}\n");
    fwrite($out, "end_of_record\n");
}

fclose($out);

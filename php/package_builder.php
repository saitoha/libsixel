#!/usr/bin/env php
<?php
/**
 * Build a bundled libsixel PHP package from an existing shared library.
 *
 * Workflow:
 * 1. Resolve libsixel shared library from --libpath or --libdir.
 * 2. Copy the library into php/src/_libs/.
 * 3. Build a platform archive into --distdir.
 * 4. Remove temporary staged files.
 *
 * @license MIT
 */

declare(strict_types=1);

const SIXEL_NAMES = ['sixel', 'libsixel', 'sixel-1', 'libsixel-1', 'msys-sixel', 'cygsixel'];

function fail(string $message): void
{
    fwrite(STDERR, "package_builder.php: {$message}\n");
    exit(1);
}

function parse_args(array $argv): array
{
    $options = [
        'distdir' => null,
        'libpath' => null,
        'libdir' => null,
        'version' => null,
        'platform' => null,
    ];

    for ($i = 1; $i < count($argv); $i++) {
        $arg = $argv[$i];
        if ($arg === '--distdir' && isset($argv[$i + 1])) {
            $options['distdir'] = $argv[++$i];
            continue;
        }
        if ($arg === '--libpath' && isset($argv[$i + 1])) {
            $options['libpath'] = $argv[++$i];
            continue;
        }
        if ($arg === '--libdir' && isset($argv[$i + 1])) {
            $options['libdir'] = $argv[++$i];
            continue;
        }
        if ($arg === '--version' && isset($argv[$i + 1])) {
            $options['version'] = $argv[++$i];
            continue;
        }
        if ($arg === '--platform' && isset($argv[$i + 1])) {
            $options['platform'] = $argv[++$i];
            continue;
        }

        fail("unknown or incomplete argument: {$arg}");
    }

    if (!is_string($options['distdir']) || $options['distdir'] === '') {
        fail('--distdir is required');
    }
    if (($options['libpath'] === null || $options['libpath'] === '')
        && ($options['libdir'] === null || $options['libdir'] === '')
    ) {
        fail('either --libpath or --libdir must be specified');
    }

    return $options;
}

/**
 * @return string|null
 */
function find_library_in_dir(string $libDir): ?string
{
    if (!is_dir($libDir)) {
        return null;
    }

    $candidates = [];
    $prefixes = ['', 'lib'];

    foreach (SIXEL_NAMES as $name) {
        foreach ($prefixes as $prefix) {
            $base = $prefix . $name;
            $patterns = [
                "{$libDir}/{$base}*.so",
                "{$libDir}/{$base}*.so.*",
                "{$libDir}/{$base}*.dylib",
                "{$libDir}/{$base}*.dll",
            ];
            foreach ($patterns as $pattern) {
                $matches = glob($pattern);
                if (!is_array($matches)) {
                    continue;
                }
                foreach ($matches as $match) {
                    $candidates[] = $match;
                }
            }
        }
    }

    sort($candidates);
    foreach (array_values(array_unique($candidates)) as $candidate) {
        if (!is_file($candidate)) {
            continue;
        }
        if (substr($candidate, -6) === '.dll.a') {
            continue;
        }
        if (substr($candidate, -8) === '.dll.def') {
            continue;
        }
        if (substr($candidate, -4) === '.lib') {
            continue;
        }
        return $candidate;
    }

    return null;
}

function normalize_version(string $version): string
{
    $version = trim($version);
    if ($version === '') {
        return '0.0.0';
    }
    $version = preg_replace('/[^0-9A-Za-z._-]+/', '.', $version);
    $version = preg_replace('/[.]{2,}/', '.', (string)$version);
    $version = trim((string)$version, '.');
    if ($version === '') {
        return '0.0.0';
    }
    if (!preg_match('/^[0-9]/', $version)) {
        $version = '0.0.0.' . $version;
    }
    return $version;
}

function detect_platform(): string
{
    $arch = php_uname('m');
    $os = defined('PHP_OS_FAMILY') ? (string)PHP_OS_FAMILY : (string)PHP_OS;
    $platform = strtolower($arch . '-' . $os);
    $platform = preg_replace('/[^a-z0-9._-]+/', '-', (string)$platform);
    return trim((string)$platform, '-');
}

function ensure_dir(string $dir): void
{
    if (is_dir($dir)) {
        return;
    }
    if (!mkdir($dir, 0777, true) && !is_dir($dir)) {
        fail("failed to create directory: {$dir}");
    }
}

function remove_tree(string $path): void
{
    if (!file_exists($path)) {
        return;
    }
    if (is_file($path) || is_link($path)) {
        @unlink($path);
        return;
    }

    $entries = scandir($path);
    if (!is_array($entries)) {
        return;
    }
    foreach ($entries as $entry) {
        if ($entry === '.' || $entry === '..') {
            continue;
        }
        remove_tree($path . DIRECTORY_SEPARATOR . $entry);
    }
    @rmdir($path);
}

function copy_file_or_tree(string $source, string $dest): void
{
    if (is_file($source)) {
        ensure_dir(dirname($dest));
        if (!copy($source, $dest)) {
            fail("copy failed: {$source} -> {$dest}");
        }
        return;
    }

    if (!is_dir($source)) {
        fail("source path not found: {$source}");
    }

    ensure_dir($dest);
    $entries = scandir($source);
    if (!is_array($entries)) {
        fail("failed to read directory: {$source}");
    }
    foreach ($entries as $entry) {
        if ($entry === '.' || $entry === '..') {
            continue;
        }
        copy_file_or_tree(
            $source . DIRECTORY_SEPARATOR . $entry,
            $dest . DIRECTORY_SEPARATOR . $entry
        );
    }
}

/**
 * @return list<string>
 */
function collect_files(string $base): array
{
    $files = [];
    $iter = new RecursiveIteratorIterator(
        new RecursiveDirectoryIterator($base, FilesystemIterator::SKIP_DOTS),
        RecursiveIteratorIterator::SELF_FIRST
    );
    /** @var SplFileInfo $node */
    foreach ($iter as $node) {
        if ($node->isDir()) {
            continue;
        }
        $files[] = $node->getPathname();
    }
    sort($files);
    return $files;
}

function build_zip(string $stageParent, string $packageBase, string $destPath): bool
{
    if (!class_exists('ZipArchive')) {
        return false;
    }

    $zip = new ZipArchive();
    if ($zip->open($destPath, ZipArchive::CREATE | ZipArchive::OVERWRITE) !== true) {
        return false;
    }

    $root = $stageParent . DIRECTORY_SEPARATOR . $packageBase;
    $prefixLen = strlen($stageParent) + 1;
    foreach (collect_files($root) as $filePath) {
        $localPath = substr($filePath, $prefixLen);
        if ($localPath === false) {
            continue;
        }
        if (!$zip->addFile($filePath, $localPath)) {
            $zip->close();
            return false;
        }
    }

    return $zip->close();
}

function build_targz(string $stageParent, string $packageBase, string $destPath): bool
{
    if (class_exists('PharData')) {
        $tarPath = substr($destPath, 0, -3);
        @unlink($tarPath);
        @unlink($destPath);
        try {
            $phar = new PharData($tarPath);
            $root = $stageParent . DIRECTORY_SEPARATOR . $packageBase;
            $prefixLen = strlen($root) + 1;
            foreach (collect_files($root) as $filePath) {
                $relative = substr($filePath, $prefixLen);
                if ($relative === false) {
                    continue;
                }
                $phar->addFile($filePath, $packageBase . '/' . $relative);
            }
            $phar->compress(Phar::GZ);
            unset($phar);
            @unlink($tarPath);
            return is_file($destPath);
        } catch (Throwable $e) {
            @unlink($tarPath);
            @unlink($destPath);
        }
    }

    $cmd = sprintf(
        'tar -C %s -czf %s %s',
        escapeshellarg($stageParent),
        escapeshellarg($destPath),
        escapeshellarg($packageBase)
    );
    exec($cmd, $output, $status);
    return ($status === 0 && is_file($destPath));
}

function main(array $argv): int
{
    $options = parse_args($argv);

    $libPath = $options['libpath'];
    if (!is_string($libPath) || $libPath === '') {
        $libDir = (string)$options['libdir'];
        $libPath = find_library_in_dir($libDir);
        if ($libPath === null) {
            fail("libsixel shared library not found in {$libDir}");
        }
    }
    $libPath = realpath($libPath);
    if (!is_string($libPath) || !is_file($libPath)) {
        fail('invalid --libpath');
    }

    $version = normalize_version((string)($options['version'] ?? '0.0.0'));
    $platform = (is_string($options['platform']) && $options['platform'] !== '')
        ? $options['platform']
        : detect_platform();
    $platform = preg_replace('/[^0-9A-Za-z._-]+/', '-', (string)$platform);
    $platform = trim((string)$platform, '-');
    if ($platform === '') {
        $platform = 'unknown';
    }

    $root = realpath(__DIR__);
    if (!is_string($root)) {
        fail('failed to resolve php/ directory');
    }
    $distDir = (string)$options['distdir'];
    ensure_dir($distDir);

    $libsDir = $root . '/src/_libs';
    ensure_dir($libsDir);
    $copiedLib = $libsDir . '/' . basename($libPath);
    if (!copy($libPath, $copiedLib)) {
        fail("failed to copy libsixel shared library: {$libPath}");
    }

    $packageBase = "libsixel-php-{$version}-{$platform}";
    $stageParent = sys_get_temp_dir() . '/libsixel-php-build-' . uniqid('', true);
    $stageRoot = $stageParent . '/' . $packageBase;

    try {
        ensure_dir($stageRoot);
        $filesToCopy = ['composer.json', 'README.md', 'LICENSE', 'src'];
        foreach ($filesToCopy as $relative) {
            $source = $root . '/' . $relative;
            if (!file_exists($source)) {
                continue;
            }
            copy_file_or_tree($source, $stageRoot . '/' . $relative);
        }
        if (!file_exists($stageRoot . '/README.md') && file_exists($root . '/README')) {
            copy_file_or_tree($root . '/README', $stageRoot . '/README');
        }

        $zipPath = rtrim($distDir, '/\\') . '/' . $packageBase . '.zip';
        $tarGzPath = rtrim($distDir, '/\\') . '/' . $packageBase . '.tar.gz';
        @unlink($zipPath);
        @unlink($tarGzPath);

        if (build_zip($stageParent, $packageBase, $zipPath)) {
            echo $zipPath . PHP_EOL;
            return 0;
        }
        if (build_targz($stageParent, $packageBase, $tarGzPath)) {
            echo $tarGzPath . PHP_EOL;
            return 0;
        }

        fail('failed to create package archive (zip and tar.gz both failed)');
    } finally {
        @unlink($copiedLib);
        $entries = scandir($libsDir);
        if (is_array($entries) && count($entries) <= 2) {
            @rmdir($libsDir);
        }
        remove_tree($stageParent);
    }
}

exit(main($argv));

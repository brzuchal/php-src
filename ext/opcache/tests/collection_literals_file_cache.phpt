--TEST--
OPcache: collection literals survive a file cache round trip
--EXTENSIONS--
opcache
zend_test
--SKIPIF--
<?php
if (!getenv('TEST_PHP_EXECUTABLE')) die('skip TEST_PHP_EXECUTABLE not set');
?>
--FILE--
<?php
$php = getenv('TEST_PHP_EXECUTABLE');
$dir = __DIR__ . '/collection_lit_fc_' . getmypid();
@mkdir($dir);
$lib = __DIR__ . '/collection_literals_shm.inc';

$args = ' -d opcache.enable=1 -d opcache.enable_cli=1'
    . ' -d opcache.file_cache=' . escapeshellarg($dir)
    . ' -d opcache.file_cache_only=1';

// The first run serializes the script -- and its descriptor table -- into the
// file cache; the second unserializes it and fixes the pointers up. Both must
// build the same values and report the same diagnostics.
$cmd = escapeshellarg($php) . $args . ' -r ' . escapeshellarg('require "' . $lib . '"; echo ocl_report();');
$cold = shell_exec($cmd);
$bins = 0;
foreach (new RecursiveIteratorIterator(new RecursiveDirectoryIterator($dir,
        FilesystemIterator::SKIP_DOTS)) as $f) {
    if ($f->isFile()) { $bins++; }
}
$warm = shell_exec($cmd);

echo "cached: ", $bins > 0 ? "yes" : "no", "\n";
echo "cold: ", trim((string) $cold), "\n";
echo "warm: ", trim((string) $warm), "\n";
echo "identical: ", var_export($cold === $warm, true), "\n";

foreach (new RecursiveIteratorIterator(new RecursiveDirectoryIterator($dir,
        FilesystemIterator::SKIP_DOTS), RecursiveIteratorIterator::CHILD_FIRST) as $f) {
    $f->isDir() ? @rmdir($f->getPathname()) : @unlink($f->getPathname());
}
@rmdir($dir);
?>
--EXPECT--
cached: yes
cold: 3|2|0|10,1|Element 1 of vec[int] must be of type int, string given|1,a,2|Element 1 of tuple[int,string] must be of type string, int given|3
warm: 3|2|0|10,1|Element 1 of vec[int] must be of type int, string given|1,a,2|Element 1 of tuple[int,string] must be of type string, int given|3
identical: true

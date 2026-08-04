--TEST--
OPcache: collection type declarations survive a file cache round trip
--EXTENSIONS--
opcache
--SKIPIF--
<?php
if (!getenv('TEST_PHP_EXECUTABLE')) die('skip TEST_PHP_EXECUTABLE not set');
?>
--FILE--
<?php
$php = getenv('TEST_PHP_EXECUTABLE');
$dir = __DIR__ . '/collection_fc_' . getmypid();
@mkdir($dir);
$lib = __DIR__ . '/collection_types_shm.inc';

$script = '$r = new ReflectionFunction("oc_f");'
    . 'echo $r->getParameters()[0]->getType(), "|", $r->getReturnType(), "|",'
    . '(new ReflectionProperty("OcC","q"))->getType(), "|",'
    . '(new ReflectionMethod("OcC","m"))->getParameters()[0]->getType(), "\n";';

$args = ' -d opcache.enable=1 -d opcache.enable_cli=1'
    . ' -d opcache.file_cache=' . escapeshellarg($dir)
    . ' -d opcache.file_cache_only=1';

// First run serializes the script into the file cache; second unserializes it.
// Run from a file rather than `php -r`: $script embeds double-quoted strings and
// $lib a drive-letter colon, both of which Windows cmd mangles inside
// escapeshellarg. A file is read directly by PHP; only its quote-free path passes
// through the shell. Kept outside $dir so it is not counted as a cache file below.
$runner = __DIR__ . '/collection_fc_run_' . getmypid() . '.php';
file_put_contents($runner, "<?php require '$lib'; $script");
$cmd = escapeshellarg($php) . $args . ' ' . escapeshellarg($runner);
$cold = shell_exec($cmd);
$bins = 0;
foreach (new RecursiveIteratorIterator(new RecursiveDirectoryIterator($dir,
        FilesystemIterator::SKIP_DOTS)) as $f) {
    if ($f->isFile()) { $bins++; }
}
$warm = shell_exec($cmd);

echo "cached: ", $bins > 0 ? "yes" : "no", "\n";
echo "cold: ", trim($cold), "\n";
echo "warm: ", trim($warm), "\n";
echo "identical: ", var_export($cold === $warm, true), "\n";

foreach (new RecursiveIteratorIterator(new RecursiveDirectoryIterator($dir,
        FilesystemIterator::SKIP_DOTS), RecursiveIteratorIterator::CHILD_FIRST) as $f) {
    $f->isDir() ? @rmdir($f->getPathname()) : @unlink($f->getPathname());
}
@unlink($runner);
@rmdir($dir);
?>
--EXPECT--
cached: yes
cold: vec[int]|vec[vec[int]]|?vec[vec[string]]|vec[vec[vec[int]]]
warm: vec[int]|vec[vec[int]]|?vec[vec[string]]|vec[vec[vec[int]]]
identical: true

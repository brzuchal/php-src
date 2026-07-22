--TEST--
OPcache: collection literals survive preloading
--INI--
opcache.enable=1
opcache.enable_cli=1
opcache.preload={PWD}/collection_literals_preload.inc
--EXTENSIONS--
opcache
zend_test
--SKIPIF--
<?php
if (PHP_OS_FAMILY == 'Windows') die('skip Preloading is not supported on Windows');
?>
--FILE--
<?php
// The literals below are compiled only by the preloaded file, so reaching them
// means the permanently stored descriptor table survived preloading. The
// descriptors are shared, read-only memory; each request resolves them again
// through its own intern tier.
var_dump(preloaded_literal_flat());

$nested = preloaded_literal_nested();
var_dump(zend_test_vec_count($nested));
var_dump(zend_test_vec_count(zend_test_vec_get($nested, 1)));
var_dump(zend_test_vec_get(zend_test_vec_get($nested, 0), 0));

echo (new PreloadedLiteralC())->m(), "\n";

// Repeated execution of a preloaded site resolves once and reuses the node.
$ids = [];
for ($i = 0; $i < 50; $i++) {
    $ids[zend_test_vec_type_id(preloaded_literal_nested())] = true;
}
var_dump(count($ids) === 1);
?>
--EXPECT--
int(3)
int(2)
int(2)
string(1) "a"
Element 1 of vec[int] must be of type int, string given
bool(true)

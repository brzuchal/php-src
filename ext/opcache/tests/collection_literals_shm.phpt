--TEST--
OPcache: collection literals survive SHM persistence and reuse
--INI--
opcache.enable=1
opcache.enable_cli=1
--EXTENSIONS--
opcache
zend_test
--FILE--
<?php
$file = __DIR__ . '/collection_literals_shm.inc';

// Persists the script -- including op_array->collection_types -- into shared
// memory and early-binds its declarations, so everything called below runs
// from the SHM copy.
var_dump(opcache_compile_file($file));

echo ocl_report(), "\n";

// Running a persisted literal repeatedly must keep producing the same
// canonical type: the descriptor lives in read-only SHM and is resolved once
// per request through the cache, never written to.
$ids = [];
for ($i = 0; $i < 100; $i++) {
    $ids[zend_test_vec_type_id(ocl_make($i))] = true;
}
var_dump(count($ids) === 1);
?>
--EXPECT--
bool(true)
3|2|0|10,1|Element 1 of vec[int] must be of type int, string given|1,a,2|Element 1 of tuple[int,string] must be of type string, int given|3
bool(true)

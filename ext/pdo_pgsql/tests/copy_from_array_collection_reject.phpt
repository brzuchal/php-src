--TEST--
F2 OQ-2: Pdo\Pgsql::copyFromArray() declares array|Traversable and rejects native collections
--EXTENSIONS--
pdo_pgsql
--SKIPIF--
<?php
require __DIR__ . '/config.inc';
require __DIR__ . '/../../../ext/pdo/tests/pdo_test.inc';
PDOTest::skip();
?>
--FILE--
<?php
require __DIR__ . '/../../../ext/pdo/tests/pdo_test.inc';
$pgsql = PDOTest::test_factory(__DIR__ . '/common.phpt', Pdo\Pgsql::class, true);
$pgsql->setAttribute(PDO::ATTR_ERRMODE, PDO::ERRMODE_SILENT);

try {
    $pgsql->copyFromArray('test_f2_copy_from', vec[string]{"a\n"});
} catch (\TypeError $e) {
    echo $e->getMessage(), PHP_EOL;
}

// The reject fires at argument validation, before any DB work: errorInfo stays clean.
var_dump($pgsql->errorInfo()[0]);
?>
--EXPECTF--
Pdo\Pgsql::copyFromArray(): Argument #2 ($rows) must be of type Traversable|array, collection given
string(5) "00000"

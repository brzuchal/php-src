--TEST--
F2 OQ-2: pg_copy_from() declares array|Traversable and rejects native collections
--EXTENSIONS--
pgsql
--SKIPIF--
<?php include("inc/skipif.inc"); ?>
--FILE--
<?php
include('inc/config.inc');
$conn = pg_connect($conn_str);
pg_query($conn, "CREATE TABLE IF NOT EXISTS test_f2_copy_from (v text)");

try {
    pg_copy_from($conn, 'test_f2_copy_from', vec[string]{"a\n"});
} catch (\TypeError $e) {
    echo $e->getMessage(), "\n";
}

pg_query($conn, "DROP TABLE test_f2_copy_from");
pg_close($conn);
?>
--EXPECT--
pg_copy_from(): Argument #3 ($rows) must be of type Traversable|array, collection given

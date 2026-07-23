--TEST--
tuple literals: shared packed lifecycle -- GC, refcounts, repeated construction
--EXTENSIONS--
zend_test
--FILE--
<?php

/* A tuple value uses the same packed storage as a vec, so destruction and GC
 * traversal are the shared paths. This checks they behave for a positional,
 * multi-member value too. */

echo "-- the same site executed many times allocates one node --\n";
$ids = [];
for ($i = 0; $i < 500; $i++) {
    $t = tuple[int, string]{$i, "s$i"};
    $ids[zend_test_vec_type_id($t)] = true;
}
var_dump(count($ids) === 1);

echo "-- repeated create/destroy without an intervening collection --\n";
for ($i = 0; $i < 2000; $i++) {
    $t = tuple[int, string, bool]{$i, 'x', true};
    unset($t);
}
echo "ok\n";

echo "-- object members are released with the value --\n";
class Tracked {
    public static int $live = 0;
    public function __construct() { self::$live++; }
    public function __destruct() { self::$live--; }
}
$t = tuple[Tracked, Tracked]{ new Tracked(), new Tracked() };
var_dump(Tracked::$live);
unset($t);
var_dump(Tracked::$live);

echo "-- a cycle through an object member is collectable --\n";
class Node { public $child; }
gc_collect_cycles();
for ($i = 0; $i < 5; $i++) {
    $n = new Node();
    $n->child = tuple[Node, int]{$n, $i};
    unset($n);
}
var_dump(gc_collect_cycles() > 0);

echo "-- a cycle through a nested tuple --\n";
gc_collect_cycles();
$n = new Node();
$n->child = tuple[tuple[Node, int], int]{ tuple[Node, int]{$n, 1}, 2 };
unset($n);
var_dump(gc_collect_cycles() > 0);

echo "-- string members released exactly once --\n";
$s = str_repeat('member', 3);   /* runtime-built, not interned */
$before = zend_test_refcount($s);
$t = tuple[string, string]{$s, $s};
var_dump(zend_test_refcount($s) - $before === 2);
unset($t);
var_dump(zend_test_refcount($s) === $before);

?>
--EXPECT--
-- the same site executed many times allocates one node --
bool(true)
-- repeated create/destroy without an intervening collection --
ok
-- object members are released with the value --
int(2)
int(0)
-- a cycle through an object member is collectable --
bool(true)
-- a cycle through a nested tuple --
bool(true)
-- string members released exactly once --
bool(true)
bool(true)

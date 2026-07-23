--TEST--
collection literals: repeated execution, repeated create/destroy, GC-visible elements
--EXTENSIONS--
zend_test
--FILE--
<?php

echo "-- the same site executed many times --\n";
$ids = [];
for ($i = 0; $i < 500; $i++) {
    $v = vec[int]{$i, $i + 1};
    $ids[zend_test_vec_type_id($v)] = true;
    if (zend_test_vec_get($v, 0) !== $i) {
        echo "wrong element at $i\n";
    }
}
/* One canonical node for the site, however often it runs. */
var_dump(count($ids) === 1);

echo "-- create and destroy without an intervening collection --\n";
for ($i = 0; $i < 2000; $i++) {
    $v = vec[string]{'a', 'b', 'c'};
    unset($v);
}
echo "ok\n";

echo "-- nested create/destroy cycles --\n";
for ($i = 0; $i < 500; $i++) {
    $v = vec[vec[int]]{ vec[int]{$i}, vec[int]{} };
    unset($v);
}
echo "ok\n";

echo "-- elements are released exactly once --\n";
/* A genuinely refcounted string. uniqid() is built at runtime and is never
 * interned; a str_repeat() of literals may be folded into an interned constant
 * under opcache, which would make the refcount checks below vacuous. */
$s = uniqid();
$before = zend_test_refcount($s);
$v = vec[string]{$s, $s};
var_dump(zend_test_refcount($s) - $before === 2);
unset($v);
var_dump(zend_test_refcount($s) === $before);

echo "-- object elements are released with the value --\n";
class Tracked {
    public static int $live = 0;
    public function __construct() { self::$live++; }
    public function __destruct() { self::$live--; }
}
$v = vec[Tracked]{ new Tracked(), new Tracked() };
var_dump(Tracked::$live);
unset($v);
var_dump(Tracked::$live);

echo "-- a cycle through an object element is collectable --\n";
class Node { public $child; }
gc_collect_cycles();
for ($i = 0; $i < 5; $i++) {
    $n = new Node();
    $n->child = vec[Node]{$n};
    unset($n);
}
var_dump(gc_collect_cycles() > 0);

echo "-- a cycle through a nested literal --\n";
gc_collect_cycles();
$n = new Node();
$n->child = vec[vec[Node]]{ vec[Node]{$n} };
unset($n);
var_dump(gc_collect_cycles() > 0);

echo "-- a literal held in an array and behind a reference --\n";
$v = vec[int]{1, 2, 3};
$holder = ['k' => $v];
$ref = &$v;
var_dump(zend_test_vec_count($holder['k']), zend_test_vec_count($ref));
unset($ref, $holder, $v);
echo "ok\n";

echo "-- a literal returned from a recursive function --\n";
function build(int $depth) {
    if ($depth === 0) {
        return vec[int]{0};
    }
    build($depth - 1);
    return vec[int]{$depth};
}
var_dump(zend_test_vec_get(build(100), 0));

?>
--EXPECT--
-- the same site executed many times --
bool(true)
-- create and destroy without an intervening collection --
ok
-- nested create/destroy cycles --
ok
-- elements are released exactly once --
bool(true)
bool(true)
-- object elements are released with the value --
int(2)
int(0)
-- a cycle through an object element is collectable --
bool(true)
-- a cycle through a nested literal --
bool(true)
-- a literal held in an array and behind a reference --
int(3)
int(3)
ok
-- a literal returned from a recursive function --
int(100)

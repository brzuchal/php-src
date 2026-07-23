--TEST--
set literals: silent deduplication, === equality, leaf-only elements
--EXTENSIONS--
zend_test
--FILE--
<?php

function fail(callable $fn): void {
    try {
        $fn();
        echo "NOT REJECTED\n";
    } catch (TypeError $ex) {
        echo $ex->getMessage(), "\n";
    }
}

function elements($s): array {
    $out = [];
    for ($i = 0; $i < zend_test_vec_count($s); $i++) {
        $out[] = zend_test_vec_get($s, $i);
    }
    return $out;
}

echo "-- duplicates are silently dropped, first occurrence kept, order preserved --\n";
$s = set[int]{3, 1, 2, 1, 3, 2, 1};
var_dump(zend_test_vec_count($s));
var_dump(elements($s));

echo "-- empty and single --\n";
var_dump(zend_test_vec_count(set[int]{}));
var_dump(zend_test_vec_count(set[int]{7}));

echo "-- scalars compared by value --\n";
var_dump(zend_test_vec_count(set[string]{'a', 'b', 'a', strtolower('B'), 'b'}));
var_dump(zend_test_vec_count(set[float]{1.5, 1.5, 2.5}));
var_dump(zend_test_vec_count(set[bool]{true, true, false, false}));

echo "-- arrays compared by value (recursively) --\n";
var_dump(zend_test_vec_count(set[array]{[1, [2]], [1, [2]], [1, [3]]}));

echo "-- objects compared by identity, not by contents --\n";
class Point { public function __construct(public int $x) {} }
$a = new Point(1);
$b = new Point(1);   // equal contents, different identity
$s = set[Point]{$a, $a, $b};
var_dump(zend_test_vec_count($s));            // $a and $b are distinct
var_dump(zend_test_vec_get($s, 0) === $a, zend_test_vec_get($s, 1) === $b);

echo "-- a dropped duplicate is not copied: refcount rises by one, not two --\n";
$str = str_repeat('unique', 3);   // runtime-built, not interned
$before = zend_test_refcount($str);
$s = set[string]{$str, $str, $str};
var_dump(zend_test_vec_count($s), zend_test_refcount($str) - $before === 1);
unset($s);
var_dump(zend_test_refcount($str) === $before);

echo "-- element type errors report the source position --\n";
fail(fn() => set[int]{1, 'x', 2});
fail(fn() => set[int]{1, 1, 'x'});          // position is 2, before dedup collapses
fail(fn() => set[Point]{new stdClass()});

echo "-- a set of collections is not constructible: no value equality yet --\n";
fail(fn() => set[vec[int]]{ vec[int]{1} });
fail(fn() => set[set[int]]{ set[int]{1} });

echo "-- unsupported leaf element types --\n";
fail(fn() => set[?int]{1});
fail(fn() => set[int|string]{1});
fail(fn() => set[mixed]{1});

echo "-- left to right, all before construction; a bad element retains side effects --\n";
function e($tag, $v) { echo "eval $tag\n"; return $v; }
$kept = 'kept';
try {
    $kept = set[int]{ e('a', 1), e('b', 'boom'), e('c', 3) };
} catch (TypeError $ex) {
    echo 'caught: ', $ex->getMessage(), "\n";
}
var_dump($kept);

echo "-- inside a closure, returned, type-checked; invariant --\n";
$make = fn(): set[int] => set[int]{1, 2, 2, 3};
var_dump(zend_test_vec_count($make()));
var_dump(zend_test_vec_type_id(set[int]{1}) === zend_test_vec_type_id(vec[int]{1}));

?>
--EXPECT--
-- duplicates are silently dropped, first occurrence kept, order preserved --
int(3)
array(3) {
  [0]=>
  int(3)
  [1]=>
  int(1)
  [2]=>
  int(2)
}
-- empty and single --
int(0)
int(1)
-- scalars compared by value --
int(2)
int(2)
int(2)
-- arrays compared by value (recursively) --
int(2)
-- objects compared by identity, not by contents --
int(2)
bool(true)
bool(true)
-- a dropped duplicate is not copied: refcount rises by one, not two --
int(1)
bool(true)
bool(true)
-- element type errors report the source position --
Element 1 of set[int] must be of type int, string given
Element 2 of set[int] must be of type int, string given
Element 0 of set[Point] must be of type Point, stdClass given
-- a set of collections is not constructible: no value equality yet --
Cannot create a value of type set[vec[int]]
Cannot create a value of type set[set[int]]
-- unsupported leaf element types --
Cannot create a value of type set[?int]
Cannot create a value of type set[string|int]
Cannot create a value of type set[mixed]
-- left to right, all before construction; a bad element retains side effects --
eval a
eval b
eval c
caught: Element 1 of set[int] must be of type int, string given
string(4) "kept"
-- inside a closure, returned, type-checked; invariant --
int(3)
bool(false)

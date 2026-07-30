--TEST--
collection identity: set membership (dedup/with/without/union/intersect/diff) uses the same value predicate
--EXTENSIONS--
zend_test
--FILE--
<?php
// Two independently allocated but value-equal vecs.
$a = vec[int]{1, 2};
$b = vec[int]{1, 2};
var_dump($a === $b);                       // true: the predicate everything below shares

echo "-- construction-time dedup by value --\n";
var_dump((set[vec[int]]{$a, $b})->count);          // 1
var_dump((set[vec[int]]{$a, $b, vec[int]{2, 1}})->count); // 2: {2,1} distinct

echo "-- with(): value-equal is a no-op; value-distinct grows --\n";
var_dump((set[vec[int]]{$a})->with($b)->count);    // 1
var_dump((set[vec[int]]{$a})->with(vec[int]{9})->count); // 2

echo "-- without(): value-equal removes; absent is a no-op --\n";
var_dump((set[vec[int]]{$a})->without($b)->isEmpty);       // true
var_dump((set[vec[int]]{$a})->without(vec[int]{9})->count); // 1

echo "-- union / intersect / diff by value --\n";
$s1 = set[vec[int]]{$a};
$s2 = set[vec[int]]{$b};                    // value-equal element, different alloc
var_dump($s1->union($s2)->count);           // 1
var_dump($s1->intersect($s2)->count);       // 1
var_dump($s1->diff($s2)->count);            // 0
var_dump($s1->diff(set[vec[int]]{vec[int]{9}})->count); // 1: nothing removed

echo "-- no-op reuse is preserved: one receiver ADDREF, no new value --\n";
$base = set[vec[int]]{vec[int]{1, 2}};
$before = zend_test_refcount($base);
$r = $base->with(vec[int]{1, 2});           // value-equal -> no-op
var_dump($r === $base);                     // true
var_dump(zend_test_refcount($base) - $before === 1); // exactly one ADDREF (held by $r)
$r2 = $base->without(vec[int]{9});          // absent -> no-op
var_dump($r2 === $base);                    // true
?>
--EXPECT--
bool(true)
-- construction-time dedup by value --
int(1)
int(2)
-- with(): value-equal is a no-op; value-distinct grows --
int(1)
int(2)
-- without(): value-equal removes; absent is a no-op --
bool(true)
int(1)
-- union / intersect / diff by value --
int(1)
int(1)
int(0)
int(1)
-- no-op reuse is preserved: one receiver ADDREF, no new value --
bool(true)
bool(true)
bool(true)

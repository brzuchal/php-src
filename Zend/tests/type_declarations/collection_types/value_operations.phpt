--TEST--
collection values: strict identity, truthiness, and type introspection
--EXTENSIONS--
zend_test
--FILE--
<?php

/* The value-surface fixes (design-audit A1/A2/A4): a collection value must be
 * safe and coherent under ordinary operations. Strict identity (A1) is now by
 * recursive value; the dedicated coverage is in collection_identity*.phpt. A2
 * (truthiness) and A4 (introspection) do not depend on it. */

echo "-- A1: strict identity is reflexive and by value --\n";
foreach ([
    'vec'   => fn() => vec[int]{1, 2},
    'tuple' => fn() => tuple[int, string]{1, 'a'},
    'set'   => fn() => set[int]{1, 2},
] as $kind => $make) {
    $a = $make();
    $b = $a;                 // same value
    $c = $make();            // separately constructed, structurally equal -> now identical
    printf("%-5s  \$a===\$a:%d  \$a===\$b:%d  \$a!==\$a:%d  \$a===\$c:%d\n",
        $kind, $a === $a, $a === $b, $a !== $a, $a === $c);
}

echo "-- A1: strict in_array / array_search match by value --\n";
$v = vec[int]{1, 2, 3};
$other = vec[int]{1, 2, 3};                        // separate instance, equal value
var_dump(in_array($v, [$other, $v], true));       // true: matches $other at index 0
var_dump(array_search($v, [7 => $other, 9 => $v], true));  // 7: first value-equal match
var_dump(in_array($v, [$other], true));           // true: value-equal, not necessarily same instance

echo "-- A1: match on the same instance --\n";
$t = tuple[int, int]{1, 2};
echo match ($t) { $t => "matched", default => "no" }, "\n";

echo "-- A2: every collection value is truthy, empty or not --\n";
var_dump(
    (bool) vec[int]{},   (bool) vec[int]{1},
    (bool) set[int]{},   (bool) set[int]{1, 1},   // dedups to one, still true
    (bool) tuple[int]{0},                          // element value 0, still true
);
if (vec[int]{}) { echo "empty vec is truthy\n"; }
if (!set[int]{}) { echo "BUG: empty set falsy\n"; } else { echo "empty set is truthy\n"; }
echo vec[int]{} ? "ternary truthy\n" : "ternary falsy\n";

echo "-- A4: gettype and get_debug_type report the coarse category --\n";
foreach ([vec[int]{1}, tuple[int, string]{1, 'a'}, set[int]{1}] as $val) {
    echo gettype($val), " / ", get_debug_type($val), "\n";
}

echo "-- A4: the full parameterised name is still rendered by var_dump --\n";
var_dump(vec[int]{1});
var_dump(tuple[int, string]{1, 'a'});
var_dump(set[string]{'x'});

?>
--EXPECT--
-- A1: strict identity is reflexive and by value --
vec    $a===$a:1  $a===$b:1  $a!==$a:0  $a===$c:1
tuple  $a===$a:1  $a===$b:1  $a!==$a:0  $a===$c:1
set    $a===$a:1  $a===$b:1  $a!==$a:0  $a===$c:1
-- A1: strict in_array / array_search match by value --
bool(true)
int(7)
bool(true)
-- A1: match on the same instance --
matched
-- A2: every collection value is truthy, empty or not --
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
empty vec is truthy
empty set is truthy
ternary truthy
-- A4: gettype and get_debug_type report the coarse category --
collection / collection
collection / collection
collection / collection
-- A4: the full parameterised name is still rendered by var_dump --
vec[int](1) {
  [0]=>
  int(1)
}
tuple[int,string](2) {
  [0]=>
  int(1)
  [1]=>
  string(1) "a"
}
set[string](1) {
  [0]=>
  string(1) "x"
}

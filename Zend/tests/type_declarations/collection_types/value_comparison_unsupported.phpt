--TEST--
collection values: non-strict comparison throws, never reaches ZEND_UNREACHABLE
--EXTENSIONS--
zend_test
--FILE--
<?php

/* design-audit A3: collections have no ordering or non-strict equality yet.
 * Every path that would have driven a collection into the scalar-coercion
 * fallback -- and thus ZEND_UNREACHABLE (abort in debug, UB in release) --
 * must throw TypeError: "Cannot compare collection values" instead.
 *
 * This test running to completion on a debug/ASAN build is itself the
 * regression proof that no ZEND_UNREACHABLE path remains. */

function must_throw(string $label, callable $fn): void {
    try {
        $fn();
        echo "$label: NOT THROWN\n";
    } catch (TypeError $e) {
        echo "$label: ", $e->getMessage(), "\n";
    }
}

$a = vec[int]{1};
$b = vec[int]{2};

echo "-- every direct comparison operator --\n";
must_throw('==',  fn() => $a == $b);
must_throw('!=',  fn() => $a != $b);
must_throw('<>',  fn() => $a <> $b);
must_throw('<',   fn() => $a < $b);
must_throw('<=',  fn() => $a <= $b);
must_throw('>',   fn() => $a > $b);
must_throw('>=',  fn() => $a >= $b);
must_throw('<=>', fn() => $a <=> $b);

echo "-- structurally equal but separately constructed still throws (not ==) --\n";
must_throw('== equal', fn() => vec[int]{1} == vec[int]{1});

echo "-- mixed operand (collection vs scalar / null / array) --\n";
must_throw('== int',   fn() => $a == 5);
must_throw('== null',  fn() => $a == null);
must_throw('== array', fn() => $a == [1]);
must_throw('< int',    fn() => $a < 5);

echo "-- across kinds --\n";
must_throw('tuple',  fn() => tuple[int]{1} == tuple[int]{1});
must_throw('set',    fn() => set[int]{1} < set[int]{2});
must_throw('mixed kinds', fn() => vec[int]{1} == tuple[int]{1});

echo "-- indirect: sort, non-strict in_array / array_search --\n";
must_throw('sort',         function () { $x = [vec[int]{2}, vec[int]{1}]; sort($x); });
must_throw('rsort',        function () { $x = [vec[int]{1}, vec[int]{2}]; rsort($x); });
must_throw('in_array',     fn() => in_array(vec[int]{1}, [vec[int]{1}]));
must_throw('array_search', fn() => array_search(vec[int]{1}, [vec[int]{1}]));

echo "-- strict identity does NOT go through comparison (no throw), and is by value --\n";
$s = set[int]{1};
var_dump($s === $s, $s !== $s);                   // true, false
var_dump($s === set[int]{1}, $s !== set[int]{1}); // separate but value-equal: true, false
var_dump(in_array($s, [$s], true));               // strict → found, no throw
var_dump($a === null);                            // strict vs null: false, no throw

?>
--EXPECT--
-- every direct comparison operator --
==: Cannot compare collection values
!=: Cannot compare collection values
<>: Cannot compare collection values
<: Cannot compare collection values
<=: Cannot compare collection values
>: Cannot compare collection values
>=: Cannot compare collection values
<=>: Cannot compare collection values
-- structurally equal but separately constructed still throws (not ==) --
== equal: Cannot compare collection values
-- mixed operand (collection vs scalar / null / array) --
== int: Cannot compare collection values
== null: Cannot compare collection values
== array: Cannot compare collection values
< int: Cannot compare collection values
-- across kinds --
tuple: Cannot compare collection values
set: Cannot compare collection values
mixed kinds: Cannot compare collection values
-- indirect: sort, non-strict in_array / array_search --
sort: Cannot compare collection values
rsort: Cannot compare collection values
in_array: Cannot compare collection values
array_search: Cannot compare collection values
-- strict identity does NOT go through comparison (no throw), and is by value --
bool(true)
bool(false)
bool(true)
bool(false)
bool(true)
bool(false)

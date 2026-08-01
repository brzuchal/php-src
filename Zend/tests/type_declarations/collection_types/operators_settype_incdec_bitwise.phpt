--TEST--
collection values: settype(), ++/--, and bitwise/shift throw instead of crashing
--FILE--
<?php

/* Sibling paths to cast_numeric.phpt. When IS_COLLECTION was added, several
 * type-juggling switches kept a `default: ZEND_UNREACHABLE()` with no
 * IS_COLLECTION arm, so a collection reaching them was undefined in a release
 * build (a crash) and aborted a debug build:
 *   - settype()   -> convert_to_long / convert_to_double / _convert_to_string /
 *                    convert_to_boolean
 *   - $c++ / $c-- -> increment_function / decrement_function
 *   - | & ^ << >>  -> zendi_try_get_long
 * Each now behaves like the equivalent array/cast operation: a TypeError, or,
 * for bool, the defined always-true conversion. */

function fail(string $label, callable $fn): void {
    try {
        $fn();
        echo "$label: NOT THROWN\n";
    } catch (TypeError $e) {
        echo "$label: ", $e->getMessage(), "\n";
    }
}

echo "-- settype(): int/float/string throw --\n";
foreach (['integer', 'double', 'string'] as $t) {
    fail("settype($t)", function () use ($t) { $c = vec[int]{1, 2}; settype($c, $t); });
}

echo "-- settype(bool) is defined (always true), even for an empty collection --\n";
$c = vec[int]{1, 2};
settype($c, 'boolean');
var_dump($c);
$e = vec[int]{};
settype($e, 'bool');
var_dump($e);

echo "-- increment / decrement throw (pre and post) --\n";
fail('$c++', function () { $c = vec[int]{1}; $c++; });
fail('++$c', function () { $c = vec[int]{1}; ++$c; });
fail('$c--', function () { $c = vec[int]{1}; $c--; });
fail('--$c', function () { $c = vec[int]{1}; --$c; });

echo "-- bitwise / shift throw (collection on either side) --\n";
fail('$c | 1', function () { $c = vec[int]{1}; return $c | 1; });
fail('1 | $c', function () { $c = vec[int]{1}; return 1 | $c; });
fail('$c & 1', function () { $c = vec[int]{1}; return $c & 1; });
fail('$c ^ 1', function () { $c = vec[int]{1}; return $c ^ 1; });
fail('$c << 1', function () { $c = vec[int]{1}; return $c << 1; });
fail('$c >> 1', function () { $c = vec[int]{1}; return $c >> 1; });

echo "-- the engine is healthy afterwards --\n";
var_dump((vec[int]{1, 2, 3})->count);
echo "OK\n";
?>
--EXPECT--
-- settype(): int/float/string throw --
settype(integer): Cannot convert a collection to int
settype(double): Cannot convert a collection to float
settype(string): Cannot convert a collection to string
-- settype(bool) is defined (always true), even for an empty collection --
bool(true)
bool(true)
-- increment / decrement throw (pre and post) --
$c++: Cannot increment collection
++$c: Cannot increment collection
$c--: Cannot decrement collection
--$c: Cannot decrement collection
-- bitwise / shift throw (collection on either side) --
$c | 1: Unsupported operand types: collection | int
1 | $c: Unsupported operand types: collection | int
$c & 1: Unsupported operand types: collection & int
$c ^ 1: Unsupported operand types: collection ^ int
$c << 1: Unsupported operand types: collection << int
$c >> 1: Unsupported operand types: collection >> int
-- the engine is healthy afterwards --
int(3)
OK

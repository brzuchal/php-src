--TEST--
collection values: numeric and string casts throw instead of crashing
--FILE--
<?php

/* (float) reached ZEND_UNREACHABLE in zval_get_double_func and crashed the
 * release build; (int) was undefined too. Both, and the string forms, now throw
 * a TypeError like the other unsupported collection operations. */

function fail(string $label, callable $fn): void {
    try {
        $fn();
        echo "$label: NOT THROWN\n";
    } catch (TypeError $e) {
        echo "$label: ", $e->getMessage(), "\n";
    }
}

$v = vec[int]{1, 2, 3};

fail('(int)',    fn() => (int) $v);
fail('(float)',  fn() => (float) $v);
fail('(string)', fn() => (string) $v);
fail('concat',   fn() => $v . 'x');
fail('interp',   fn() => "$v");
fail('+ int',    fn() => $v + 1);
fail('intdiv',   fn() => intdiv($v, 2));

echo "-- (bool) is defined (always true) and does not throw --\n";
var_dump((bool) $v, (bool) vec[int]{});

?>
--EXPECT--
(int): Cannot convert a collection to int
(float): Cannot convert a collection to float
(string): Cannot convert a collection to string
concat: Cannot convert a collection to string
interp: Cannot convert a collection to string
+ int: Unsupported operand types: collection + int
intdiv: intdiv(): Argument #1 ($num1) must be of type int, collection given
-- (bool) is defined (always true) and does not throw --
bool(true)
bool(true)

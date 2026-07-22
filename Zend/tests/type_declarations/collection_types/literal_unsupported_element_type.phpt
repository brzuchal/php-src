--TEST--
collection literals: element types that have no value representation
--EXTENSIONS--
zend_test
--FILE--
<?php

/* A parameter the type system accepts is not automatically a parameter a
 * *value* may hold. Whether it is, is decided by promotion -- for a nested
 * parameter the answer is a property of the promoted child node -- so it is
 * reported on the first execution that reaches the site, not while compiling. */

function fail(callable $fn): void {
    try {
        $fn();
        echo "NOT REJECTED\n";
    } catch (TypeError $ex) {
        echo $ex->getMessage(), "\n";
    }
}

echo "-- nullable element types --\n";
fail(fn() => vec[?int]{1});
fail(fn() => vec[?int]{});
fail(fn() => vec[?string]{'a'});

echo "-- union element types --\n";
fail(fn() => vec[int|string]{1});
fail(fn() => vec[int|null]{1});

echo "-- intersection element types --\n";
interface A {}
interface B {}
fail(fn() => vec[A&B]{});

echo "-- mask-encoded pseudo types --\n";
fail(fn() => vec[mixed]{1});
fail(fn() => vec[object]{new stdClass()});
fail(fn() => vec[callable]{'strlen'});
fail(fn() => vec[null]{null});
fail(fn() => vec[false]{false});

echo "-- nesting an unsupported parameter --\n";
fail(fn() => vec[vec[?int]]{});

echo "-- rejected before any element is looked at --\n";
function e($v) { echo "evaluated\n"; return $v; }
fail(fn() => vec[?int]{ e(1), e(2) });

echo "-- the supported ones still work --\n";
var_dump(zend_test_vec_count(vec[int]{1}));
var_dump(zend_test_vec_count(vec[stdClass]{new stdClass()}));

?>
--EXPECT--
-- nullable element types --
Cannot create a value of type vec[?int]
Cannot create a value of type vec[?int]
Cannot create a value of type vec[?string]
-- union element types --
Cannot create a value of type vec[string|int]
Cannot create a value of type vec[?int]
-- intersection element types --
Cannot create a value of type vec[A&B]
-- mask-encoded pseudo types --
Cannot create a value of type vec[mixed]
Cannot create a value of type vec[object]
Cannot create a value of type vec[callable]
Cannot create a value of type vec[null]
Cannot create a value of type vec[false]
-- nesting an unsupported parameter --
Cannot create a value of type vec[vec[?int]]
-- rejected before any element is looked at --
evaluated
evaluated
Cannot create a value of type vec[?int]
-- the supported ones still work --
int(1)
int(1)

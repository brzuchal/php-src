--TEST--
collection literals: element type errors name the position, the type and the value
--EXTENSIONS--
zend_test
--FILE--
<?php

class Foo {}
class Bar extends Foo {}
interface Iface {}
class Impl implements Iface {}

function fail(callable $fn): void {
    try {
        $fn();
        echo "NOT REJECTED\n";
    } catch (TypeError $ex) {
        echo $ex->getMessage(), "\n";
    }
}

echo "-- builtin element types --\n";
fail(fn() => vec[int]{'x'});
fail(fn() => vec[int]{1, 2, null});
fail(fn() => vec[string]{1});
fail(fn() => vec[float]{'1.5'});
fail(fn() => vec[bool]{1});
fail(fn() => vec[array]{new stdClass()});

echo "-- the position is the element index --\n";
fail(fn() => vec[int]{1, 2, 3, 4, 'here'});

echo "-- int is not silently widened to float, and there is no juggling --\n";
fail(fn() => vec[float]{1});
fail(fn() => vec[int]{'7'});

echo "-- class element types --\n";
fail(fn() => vec[Foo]{new stdClass()});
fail(fn() => vec[Foo]{'Foo'});
fail(fn() => vec[Iface]{new Foo()});

echo "-- a subclass is accepted, a superclass is not --\n";
$ok = vec[Foo]{new Bar()};
var_dump(zend_test_vec_count($ok));
$ok2 = vec[Iface]{new Impl()};
var_dump(zend_test_vec_count($ok2));
fail(fn() => vec[Bar]{new Foo()});

echo "-- an unknown class matches nothing --\n";
fail(fn() => vec[NoSuchClass]{new Foo()});

echo "-- a collection given where a scalar element is declared --\n";
fail(fn() => vec[int]{ vec[int]{1} });

echo "-- strict_types does not apply: element types are invariant either way --\n";
fail(fn() => vec[int]{1.0});

?>
--EXPECT--
-- builtin element types --
Element 0 of vec[int] must be of type int, string given
Element 2 of vec[int] must be of type int, null given
Element 0 of vec[string] must be of type string, int given
Element 0 of vec[float] must be of type float, string given
Element 0 of vec[bool] must be of type bool, int given
Element 0 of vec[array] must be of type array, stdClass given
-- the position is the element index --
Element 4 of vec[int] must be of type int, string given
-- int is not silently widened to float, and there is no juggling --
Element 0 of vec[float] must be of type float, int given
Element 0 of vec[int] must be of type int, string given
-- class element types --
Element 0 of vec[Foo] must be of type Foo, stdClass given
Element 0 of vec[Foo] must be of type Foo, string given
Element 0 of vec[Iface] must be of type Iface, Foo given
-- a subclass is accepted, a superclass is not --
int(1)
int(1)
Element 0 of vec[Bar] must be of type Bar, Foo given
-- an unknown class matches nothing --
Element 0 of vec[NoSuchClass] must be of type NoSuchClass, Foo given
-- a collection given where a scalar element is declared --
Element 0 of vec[int] must be of type int, vec[int] given
-- strict_types does not apply: element types are invariant either way --
Element 0 of vec[int] must be of type int, float given

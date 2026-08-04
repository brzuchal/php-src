--TEST--
tuple literals: positional construction, exact arity, per-position type checks
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

echo "-- basic, heterogeneous --\n";
$t = tuple[int, string]{1, 'a'};
var_dump(zend_test_vec_count($t), zend_test_vec_get($t, 0), zend_test_vec_get($t, 1));

$three = tuple[int, string, bool]{7, 'x', true};
var_dump(zend_test_vec_count($three), zend_test_vec_get($three, 2));

echo "-- a one-element tuple is distinct from a vec --\n";
$one = tuple[int]{5};
var_dump(zend_test_vec_count($one), zend_test_vec_get($one, 0));
var_dump(zend_test_vec_type_id($one) === zend_test_vec_type_id(vec[int]{5}));

echo "-- two tuples of the same shape share the canonical node --\n";
var_dump(zend_test_vec_type_id(tuple[int, string]{1, 'a'})
      === zend_test_vec_type_id(tuple[int, string]{9, 'z'}));
echo "-- a different order is a different type --\n";
var_dump(zend_test_vec_type_id(tuple[int, string]{1, 'a'})
      === zend_test_vec_type_id(tuple[string, int]{'a', 1}));

echo "-- arity is exact, checked at compile time --\n";
foreach ([
    'too few'  => 'tuple[int, string]{1};',
    'too many' => 'tuple[int]{1, 2};',
    'empty'    => 'tuple[int]{};',
] as $label => $code) {
    $out = trim((string) shell_exec(getenv('TEST_PHP_EXECUTABLE_ESCAPED')
        . ' -n -r ' . escapeshellarg('$x = ' . $code) . ' 2>&1'));
    echo $label, ': ', trim(explode("\n", $out)[0]), "\n";
}

echo "-- per-position type errors name the position and that member's type --\n";
fail(fn() => tuple[int, string]{'x', 'y'});      // element 0
fail(fn() => tuple[int, string]{1, 2});          // element 1
fail(fn() => tuple[int, string, bool]{1, 'a', 0}); // element 2

echo "-- class and interface members, positionally --\n";
interface I {}
class C implements I {}
$tc = tuple[I, int]{new C(), 3};
var_dump(zend_test_vec_count($tc));
fail(fn() => tuple[I, int]{new stdClass(), 3});
fail(fn() => tuple[int, C]{1, new stdClass()});

echo "-- nested collections as members --\n";
$nested = tuple[vec[int], tuple[int, int]]{ vec[int]{1, 2}, tuple[int, int]{3, 4} };
var_dump(zend_test_vec_count($nested));
var_dump(zend_test_vec_count(zend_test_vec_get($nested, 0)));
var_dump(zend_test_vec_get(zend_test_vec_get($nested, 1), 1));
fail(fn() => tuple[vec[int], int]{ vec[string]{'a'}, 2 });

echo "-- left to right, all before construction; failure retains side effects --\n";
function e(string $tag, $v) { echo "eval $tag\n"; return $v; }
$before = 'kept';
try {
    $before = tuple[int, int]{ e('a', 1), e('b', 'boom') };
} catch (TypeError $ex) {
    echo 'caught: ', $ex->getMessage(), "\n";
}
var_dump($before);

echo "-- unsupported member type is a runtime error --\n";
fail(fn() => tuple[int, ?string]{1, 'a'});
fail(fn() => tuple[int|string, int]{1, 2});

echo "-- inside a closure, returned, type-checked --\n";
$make = fn(): tuple[int, string] => tuple[int, string]{42, 'answer'};
$r = $make();
var_dump(zend_test_vec_get($r, 0), zend_test_vec_get($r, 1));

?>
--EXPECT--
-- basic, heterogeneous --
int(2)
int(1)
string(1) "a"
int(3)
bool(true)
-- a one-element tuple is distinct from a vec --
int(1)
int(5)
bool(false)
-- two tuples of the same shape share the canonical node --
bool(true)
-- a different order is a different type --
bool(false)
-- arity is exact, checked at compile time --
too few: Fatal error: Collection type tuple expects 2 elements, 1 given in Command line code on line 1
too many: Fatal error: Collection type tuple expects 1 element, 2 given in Command line code on line 1
empty: Fatal error: Collection type tuple expects 1 element, 0 given in Command line code on line 1
-- per-position type errors name the position and that member's type --
Element 0 of tuple[int,string] must be of type int, string given
Element 1 of tuple[int,string] must be of type string, int given
Element 2 of tuple[int,string,bool] must be of type bool, int given
-- class and interface members, positionally --
int(2)
Element 0 of tuple[I,int] must be of type I, stdClass given
Element 1 of tuple[int,C] must be of type C, stdClass given
-- nested collections as members --
int(2)
int(2)
int(4)
Element 0 of tuple[vec[int],int] must be of type vec[int], vec[string] given
-- left to right, all before construction; failure retains side effects --
eval a
eval b
caught: Element 1 of tuple[int,int] must be of type int, string given
string(4) "kept"
-- unsupported member type is a runtime error --
Cannot create a value of type tuple[int,?string]
Cannot create a value of type tuple[string|int,int]
-- inside a closure, returned, type-checked --
int(42)
string(6) "answer"

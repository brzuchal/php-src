--TEST--
collection literals: nested literals and nested descriptors
--EXTENSIONS--
zend_test
--FILE--
<?php

echo "-- a literal as an element of a literal --\n";
$outer = vec[vec[int]]{ vec[int]{1, 2}, vec[int]{3} };
var_dump(zend_test_vec_count($outer));
$first = zend_test_vec_get($outer, 0);
$second = zend_test_vec_get($outer, 1);
var_dump(zend_test_vec_count($first), zend_test_vec_count($second));
var_dump(zend_test_vec_get($first, 1), zend_test_vec_get($second, 0));

echo "-- the inner values borrow the same node as a standalone one --\n";
$standalone = vec[int]{9};
var_dump(zend_test_vec_type_id($first) === zend_test_vec_type_id($standalone));
var_dump(zend_test_vec_type_id($outer) === zend_test_vec_type_id($first));

echo "-- three levels --\n";
$deep = vec[vec[vec[int]]]{ vec[vec[int]]{ vec[int]{7} } };
var_dump(
    zend_test_vec_count($deep),
    zend_test_vec_count(zend_test_vec_get($deep, 0)),
    zend_test_vec_get(zend_test_vec_get(zend_test_vec_get($deep, 0), 0), 0),
);

echo "-- four levels --\n";
$deeper = vec[vec[vec[vec[string]]]]{ vec[vec[vec[string]]]{ vec[vec[string]]{ vec[string]{'leaf'} } } };
$cur = $deeper;
for ($i = 0; $i < 4; $i++) {
    $cur = zend_test_vec_get($cur, 0);
}
var_dump($cur);

echo "-- an empty nested literal --\n";
$emptyNested = vec[vec[int]]{ vec[int]{} };
var_dump(zend_test_vec_count($emptyNested), zend_test_vec_count(zend_test_vec_get($emptyNested, 0)));

echo "-- a nested element of the wrong parameter type --\n";
try {
    $bad = vec[vec[int]]{ vec[string]{'a'} };
} catch (TypeError $ex) {
    echo $ex->getMessage(), "\n";
}

echo "-- nesting is invariant, not covariant --\n";
$ints = vec[int]{1};
try {
    $bad = vec[vec[vec[int]]]{ vec[int]{1} };
} catch (TypeError $ex) {
    echo $ex->getMessage(), "\n";
}

echo "-- a value built elsewhere is accepted as a nested element --\n";
$reused = vec[vec[int]]{ $ints, vec[int]{2} };
var_dump(zend_test_vec_count($reused), zend_test_vec_get(zend_test_vec_get($reused, 0), 0));

?>
--EXPECT--
-- a literal as an element of a literal --
int(2)
int(2)
int(1)
int(2)
int(3)
-- the inner values borrow the same node as a standalone one --
bool(true)
bool(false)
-- three levels --
int(1)
int(1)
int(7)
-- four levels --
string(4) "leaf"
-- an empty nested literal --
int(1)
int(0)
-- a nested element of the wrong parameter type --
Element 0 of vec[vec[int]] must be of type vec[int], vec[string] given
-- nesting is invariant, not covariant --
Element 0 of vec[vec[vec[int]]] must be of type vec[vec[int]], vec[int] given
-- a value built elsewhere is accepted as a nested element --
int(2)
int(1)

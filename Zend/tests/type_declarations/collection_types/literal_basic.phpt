--TEST--
collection literals: empty, single and multiple elements
--EXTENSIONS--
zend_test
--FILE--
<?php

$empty = vec[int]{};
var_dump(zend_test_vec_count($empty));

$one = vec[int]{42};
var_dump(zend_test_vec_count($one), zend_test_vec_get($one, 0));

$many = vec[int]{1, 2, 3, 4, 5};
var_dump(zend_test_vec_count($many));
$seen = [];
for ($i = 0; $i < zend_test_vec_count($many); $i++) {
    $seen[] = zend_test_vec_get($many, $i);
}
echo implode(' ', $seen), "\n";

/* A trailing comma is allowed, as it is everywhere else. */
$trailing = vec[string]{'a', 'b',};
var_dump(zend_test_vec_count($trailing), zend_test_vec_get($trailing, 1));

/* Every supported builtin element type. */
$strings = vec[string]{'x', 'y'};
$floats  = vec[float]{1.5, 2.5};
$bools   = vec[bool]{true, false};
$arrays  = vec[array]{[1], ['k' => 'v']};
var_dump(
    zend_test_vec_get($strings, 0),
    zend_test_vec_get($floats, 1),
    zend_test_vec_get($bools, 0),
    zend_test_vec_get($arrays, 1),
);

/* Two literals spelled the same way borrow the same canonical node: the type
 * is canonicalized, not copied per site. */
var_dump(zend_test_vec_type_id($one) === zend_test_vec_type_id($many));
var_dump(zend_test_vec_type_id($one) === zend_test_vec_type_id($strings));

/* An empty literal is typed too. */
var_dump(zend_test_vec_type_id($empty) === zend_test_vec_type_id($one));

/* A collection is not an array, and it satisfies its declared type. */
var_dump(is_array($many), $many instanceof stdClass);
function want(vec[int] $v): int { return zend_test_vec_count($v); }
var_dump(want(vec[int]{1, 2}));

?>
--EXPECT--
int(0)
int(1)
int(42)
int(5)
1 2 3 4 5
int(2)
string(1) "b"
string(1) "x"
float(2.5)
bool(true)
array(1) {
  ["k"]=>
  string(1) "v"
}
bool(true)
bool(false)
bool(true)
bool(false)
bool(false)
int(2)

--TEST--
collection identity: set equality is order-insensitive; physical order stays observable via foreach
--FILE--
<?php
// Order-insensitive: same elements, any physical order -> identical.
var_dump(set[int]{1, 2} === set[int]{2, 1});          // true
var_dump(set[int]{1, 2, 3} === set[int]{3, 1, 2});    // true
var_dump(set[int]{1, 2} === set[int]{1, 3});          // false: different element
var_dump(set[int]{1, 2, 3} === set[int]{1, 2});       // false: different count
var_dump(set[int]{} === set[int]{});                  // true: empty
var_dump(set[string]{'a', 'b'} !== set[string]{'b', 'a'}); // false (they ARE equal)

// Equal count but different members is not identity.
var_dump(set[int]{1, 2} === set[int]{3, 4});          // false

// Physical iteration order is NOT part of value identity, but stays observable.
$s = set[int]{3, 1, 2};
$order = [];
foreach ($s as $x) { $order[] = $x; }
var_dump($order);                                     // [3, 1, 2] insertion order
var_dump($s === set[int]{1, 2, 3});                   // true regardless of that order
?>
--EXPECT--
bool(true)
bool(true)
bool(false)
bool(false)
bool(true)
bool(false)
bool(false)
array(3) {
  [0]=>
  int(3)
  [1]=>
  int(1)
  [2]=>
  int(2)
}
bool(true)

--TEST--
collection identity: vec/tuple are positional value types (order significant, short-circuit)
--FILE--
<?php
// vec: descriptor + count + positional elements, order significant.
var_dump(vec[int]{1, 2} === vec[int]{1, 2});      // true
var_dump(vec[int]{1, 2} === vec[int]{2, 1});      // false: order
var_dump(vec[int]{1, 2} === vec[int]{1, 2, 3});   // false: count
var_dump(vec[int]{1, 2} !== vec[int]{1, 3});      // true
var_dump(vec[int]{} === vec[int]{});              // true: two empties

// reflexive even when equality would otherwise be expensive
$v = vec[int]{1, 2, 3};
var_dump($v === $v, $v !== $v);                   // true, false

// tuple: fixed arity, positional, slot order significant.
var_dump(tuple[string, int]{'A', 12} === tuple[string, int]{'A', 12}); // true
var_dump(tuple[int, int]{1, 2} === tuple[int, int]{2, 1});             // false: slot order
var_dump(tuple[int, string]{1, 'a'} !== tuple[int, string]{1, 'b'});   // true

// a vec and a tuple with the same contents are different kinds -> never identical
var_dump(vec[int]{1} === tuple[int]{1});          // false
?>
--EXPECT--
bool(true)
bool(false)
bool(false)
bool(true)
bool(true)
bool(true)
bool(false)
bool(true)
bool(false)
bool(true)
bool(false)

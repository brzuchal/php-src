--TEST--
collection identity: different kinds/descriptors never identical; collection vs non-collection never identical
--FILE--
<?php
// Exact canonical descriptor required: vec[int] and vec[float] are different types.
var_dump(vec[int]{1} === vec[float]{1.0});    // false: descriptor (int vs float)
var_dump(vec[int]{1} !== vec[float]{1.0});    // true

// Different kinds are different descriptors.
var_dump(vec[int]{1} === set[int]{1});        // false: vec vs set
var_dump(vec[int]{1} === tuple[int]{1});      // false: vec vs tuple
var_dump(set[int]{1} === tuple[int]{1});      // false: set vs tuple

// A collection is never identical to any non-collection value.
$v = vec[int]{1, 2};
var_dump($v === 5);          // false
var_dump($v === 'vec');      // false
var_dump($v === null);       // false
var_dump($v === false);      // false
var_dump($v === [1, 2]);     // false: array, not a collection
var_dump($v === new stdClass); // false
var_dump($v !== null);       // true

// ...and symmetric on the other side.
var_dump([1, 2] === $v, null === $v, 5 === $v); // false, false, false
?>
--EXPECT--
bool(false)
bool(true)
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
bool(true)
bool(false)
bool(false)
bool(false)

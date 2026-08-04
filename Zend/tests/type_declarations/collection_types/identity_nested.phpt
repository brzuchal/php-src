--TEST--
collection identity: nested collections recurse; nested set stays order-insensitive; deep nesting
--FILE--
<?php
// Nested vec inside tuple: recurse into the element.
var_dump(tuple[vec[int], int]{vec[int]{1, 2}, 9} === tuple[vec[int], int]{vec[int]{1, 2}, 9}); // true
var_dump(tuple[vec[int], int]{vec[int]{1, 2}, 9} === tuple[vec[int], int]{vec[int]{2, 1}, 9}); // false: inner order
var_dump(tuple[vec[int], int]{vec[int]{1, 2}, 9} === tuple[vec[int], int]{vec[int]{1, 2}, 8}); // false: outer slot

// vec of vec.
var_dump(vec[vec[int]]{vec[int]{1}, vec[int]{2}} === vec[vec[int]]{vec[int]{1}, vec[int]{2}}); // true
var_dump(vec[vec[int]]{vec[int]{1}, vec[int]{2}} === vec[vec[int]]{vec[int]{2}, vec[int]{1}}); // false: outer order

// A nested SET element is compared order-insensitively even inside a positional vec.
var_dump(vec[set[int]]{set[int]{1, 2}} === vec[set[int]]{set[int]{2, 1}}); // true
var_dump(vec[set[int]]{set[int]{1, 2}} === vec[set[int]]{set[int]{1, 3}}); // false

// set of sets: nested value identity + outer order-insensitivity.
var_dump(set[set[int]]{set[int]{1, 2}, set[int]{3}} === set[set[int]]{set[int]{3}, set[int]{2, 1}}); // true

// Deep but finite nesting recurses correctly (equal vs leaf-differs).
$mk = fn($leaf) => vec[vec[vec[int]]]{ vec[vec[int]]{ vec[int]{$leaf, 2} } };
var_dump($mk(1) === $mk(1));   // true
var_dump($mk(1) === $mk(9));   // false: deepest leaf differs
?>
--EXPECT--
bool(true)
bool(false)
bool(false)
bool(true)
bool(false)
bool(true)
bool(false)
bool(true)
bool(true)
bool(false)

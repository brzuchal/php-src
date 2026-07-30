--TEST--
vec/tuple indexed read: $v[$i] / $t[$i] yield the element at a strict int position
--FILE--
<?php
$v = vec[int]{10, 20, 30};
$t = tuple[int, string, bool]{1, "a", true};

// first / middle / last
var_dump($v[0], $v[1], $v[2]);
var_dump($t[0], $t[1], $t[2]);

// single-element
$one = vec[int]{7};
var_dump($one[0]);

// variable and computed indices
$i = 1;
var_dump($v[$i], $v[$i + 1], $v[2 - 2]);

// heterogeneous tuple keeps each position's runtime value/type
var_dump(get_debug_type($t[0]), get_debug_type($t[1]), get_debug_type($t[2]));

// reading does not mutate the receiver
var_dump($v->count, $t->count);
?>
--EXPECT--
int(10)
int(20)
int(30)
int(1)
string(1) "a"
bool(true)
int(7)
int(20)
int(30)
int(10)
string(3) "int"
string(6) "string"
string(4) "bool"
int(3)
int(3)

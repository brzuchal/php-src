--TEST--
vec: append() and prepend() basic semantics (immutable, exact descriptor)
--FILE--
<?php
$v = vec[int]{1, 2};
$a = $v->append(3);
$p = $v->prepend(0);

var_dump($a);                 // vec[int]{1,2,3}
var_dump($p);                 // vec[int]{0,1,2}
var_dump($v);                 // unchanged: vec[int]{1,2}
var_dump(get_debug_type($a)); // exact descriptor is a collection

$s = vec[string]{"a"};
var_dump($s->append("b"));    // vec[string]{"a","b"}

// temporary receiver returned from a function
function make(): vec[int] { return vec[int]{7, 8}; }
var_dump(make()->append(9));  // vec[int]{7,8,9}

// chaining on method results (each step is a new value)
var_dump($v->append(3)->prepend(0));  // vec[int]{0,1,2,3}
?>
--EXPECT--
vec[int](3) {
  [0]=>
  int(1)
  [1]=>
  int(2)
  [2]=>
  int(3)
}
vec[int](3) {
  [0]=>
  int(0)
  [1]=>
  int(1)
  [2]=>
  int(2)
}
vec[int](2) {
  [0]=>
  int(1)
  [1]=>
  int(2)
}
string(10) "collection"
vec[string](2) {
  [0]=>
  string(1) "a"
  [1]=>
  string(1) "b"
}
vec[int](3) {
  [0]=>
  int(7)
  [1]=>
  int(8)
  [2]=>
  int(9)
}
vec[int](4) {
  [0]=>
  int(0)
  [1]=>
  int(1)
  [2]=>
  int(2)
  [3]=>
  int(3)
}

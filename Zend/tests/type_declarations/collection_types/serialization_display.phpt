--TEST--
collection values: var_export literal syntax, var_dump / print_r / debug_zval_dump contents
--EXTENSIONS--
zend_test
--FILE--
<?php

echo "== var_export emits literal syntax ==\n";
var_export(vec[int]{1, 2, 3});
echo "\n";
var_export(tuple[int, string]{1, 'a'});
echo "\n";

echo "\n-- and it evals back to an equal value (incl. nested) --\n";
foreach ([vec[int]{1, 2, 3}, tuple[int, string]{5, 'x'}, set[int]{1, 2, 2},
          vec[vec[int]]{ vec[int]{1}, vec[int]{2, 3} }] as $v) {
    $back = eval('return ' . var_export($v, true) . ';');
    var_dump(zend_test_vec_count($back) === zend_test_vec_count($v)
          && zend_test_vec_type_id($back) === zend_test_vec_type_id($v));
}

echo "\n== var_dump shows the type, count and elements ==\n";
var_dump(vec[int]{1, 2, 3});
var_dump(tuple[int, string]{7, 'q'});
var_dump(vec[vec[int]]{ vec[int]{1}, vec[int]{2, 3} });
var_dump(vec[int]{});

echo "== print_r ==\n";
print_r(vec[int]{1, 2, 3});
print_r(set[string]{'a', 'b'});

echo "== debug_zval_dump (with refcount) ==\n";
$v = vec[int]{1, 2};
debug_zval_dump($v);

?>
--EXPECT--
== var_export emits literal syntax ==
vec[int]{
  1,
  2,
  3,
}
tuple[int,string]{
  1,
  'a',
}

-- and it evals back to an equal value (incl. nested) --
bool(true)
bool(true)
bool(true)
bool(true)

== var_dump shows the type, count and elements ==
vec[int](3) {
  [0]=>
  int(1)
  [1]=>
  int(2)
  [2]=>
  int(3)
}
tuple[int,string](2) {
  [0]=>
  int(7)
  [1]=>
  string(1) "q"
}
vec[vec[int]](2) {
  [0]=>
  vec[int](1) {
    [0]=>
    int(1)
  }
  [1]=>
  vec[int](2) {
    [0]=>
    int(2)
    [1]=>
    int(3)
  }
}
vec[int](0) {
}
== print_r ==
vec[int]
(
    [0] => 1
    [1] => 2
    [2] => 3
)
set[string]
(
    [0] => a
    [1] => b
)
== debug_zval_dump (with refcount) ==
vec[int](2) refcount(2){
  [0]=>
  int(1)
  [1]=>
  int(2)
}

--TEST--
collection types: runtime dispatch is explicit, never UNKNOWN or silently wrong
--EXTENSIONS--
zend_test
--FILE--
<?php
zend_test_make_vec([1, 2, 3], 'int', $v);

/* Display now shows the type, count and contents -- never UNKNOWN:0. */
var_dump($v);

/* Serialization is a real round-trip, not a silent zero or a throw. */
$s = serialize($v);
echo $s, "\n";
$u = unserialize($s);
var_dump(zend_test_vec_count($u) === 3, zend_test_vec_type_id($u) === zend_test_vec_type_id($v));

/* var_export is source-level literal syntax that evals back. */
echo var_export($v, true), "\n";

/* Operations with genuinely no meaning still fail loudly rather than producing
 * a value that would silently round-trip to something else. */
foreach ([
    'string cast' => fn() => (string) $v,
    'int cast'    => fn() => (int) $v,
] as $label => $op) {
    try { $op(); echo "$label: NO ERROR\n"; }
    catch (TypeError $e) { echo "$label: ", $e->getMessage(), "\n"; }
}

/* Ordinary values are entirely unaffected. */
var_dump(1, "s", [1], null, true, 1.5);
echo serialize([1, 2]), "\n";
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
L:vec:1:{i;}:3:{i:1;i:2;i:3;}
bool(true)
bool(true)
vec[int]{
  1,
  2,
  3,
}
string cast: Cannot convert a collection to string
int cast: Cannot convert a collection to int
int(1)
string(1) "s"
array(1) {
  [0]=>
  int(1)
}
NULL
bool(true)
float(1.5)
a:2:{i:0;i:1;i:1;i:2;}

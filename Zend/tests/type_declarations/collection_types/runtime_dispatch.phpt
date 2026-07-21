--TEST--
collection types: runtime dispatch is explicit, never UNKNOWN or silently wrong
--EXTENSIONS--
zend_test
--FILE--
<?php
zend_test_make_vec([1, 2, 3], 'int', $v);

/* Debug output reports the real runtime type instead of UNKNOWN:0.
 * The exact rendering below is provisional: collections have no display
 * semantics yet, so this asserts only that the type is reported and that
 * nothing falls through to UNKNOWN. It is not a public format, and it
 * deliberately shows neither element count nor contents. */
var_dump($v);
debug_zval_dump($v);

/* Operations with no defined semantics fail loudly rather than producing a
 * value that would silently round-trip to something else. */
foreach ([
    'var_export' => fn() => var_export($v, true),
    'serialize'  => fn() => serialize($v),
    'string cast'=> fn() => (string) $v,
] as $label => $op) {
    try { $op(); echo "$label: NO ERROR\n"; }
    catch (TypeError $e) { echo "$label: ", get_class($e), ": ", $e->getMessage(), "\n"; }
}

/* Ordinary values are entirely unaffected. */
var_dump(1, "s", [1], null, true, 1.5);
echo var_export([1, 'a' => null], true), "\n";
echo serialize([1, 2]), "\n";
?>
--EXPECT--
vec[int]
vec[int]
var_export: TypeError: Cannot export a collection value
serialize: TypeError: Cannot serialize a collection value
string cast: TypeError: Cannot convert a collection to string
int(1)
string(1) "s"
array(1) {
  [0]=>
  int(1)
}
NULL
bool(true)
float(1.5)
array (
  0 => 1,
  'a' => NULL,
)
a:2:{i:0;i:1;i:1;i:2;}

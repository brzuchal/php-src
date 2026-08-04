--TEST--
Bare collection-kind types: typed by-reference parameters
--FILE--
<?php
function byRef(vec[] &$x): void { echo $x->count, "\n"; }

$v = vec[int]{1, 2, 3, 4};
byRef($v);                          // 4

$w = set[int]{1};
try { byRef($w); }                  // wrong kind
catch (\TypeError $e) { echo $e->getMessage(), "\n"; }
?>
--EXPECTF--
4
byRef(): Argument #1 ($x) must be of type vec[], set[int] given, called in %s on line %d

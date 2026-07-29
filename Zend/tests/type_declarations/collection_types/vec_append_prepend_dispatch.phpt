--TEST--
vec: append()/prepend() dispatch surfaces (dynamic name, case-insensitive, references)
--FILE--
<?php
$v = vec[int]{1, 2};

$m = 'append';  var_dump($v->$m(3)->count === 3);   // dynamic method name
$m = 'APPEND';  var_dump($v->$m(3)->count === 3);   // case-insensitive
$m = 'PrEpEnD'; var_dump($v->$m(0)->count === 3);

$r =& $v;       var_dump($r->append(3)->count === 3);       // reference receiver
$r1 =& $v; $r2 =& $r1; var_dump($r2->prepend(0)->count === 3); // chained references

var_dump($v->count === 2);                            // receiver never mutated
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)

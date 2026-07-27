--TEST--
Bare collection-kind types: return type + runtime descriptor preservation
--FILE--
<?php
function identity(vec[] $v): vec[] { return $v; }

$r = identity(vec[int]{1, 2, 3});
var_dump($r);                       // still the concrete vec[int]
echo $r->count, "\n";               // intrinsic property works: 3
echo serialize($r), "\n";           // vec[int] wire form preserved

// wrong-kind return rejected
function bad(): vec[] { return set[int]{1}; }
try { bad(); } catch (\TypeError $e) { echo $e->getMessage(), "\n"; }
?>
--EXPECTF--
vec[int](3) {
  [0]=>
  int(1)
  [1]=>
  int(2)
  [2]=>
  int(3)
}
3
L:vec:1:{i;}:3:{i:1;i:2;i:3;}
bad(): Return value must be of type vec[], set[int] returned

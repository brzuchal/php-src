--TEST--
Collection intrinsic methods: argument unpacking keeps the header receiver intact
--FILE--
<?php
$v = vec[int]{1, 2};
var_dump($v->__receiverProbe(...[]) === $v);       // empty unpack
var_dump($v->__receiverProbe(...[7]) === $v);      // one arg
// Large unpack forces a stack-segment reallocation; the header receiver survives
// and the arity error still reports the receiver-free public arity:
try { $v->__receiverProbe(...range(1, 40)); }
catch (\ArgumentCountError $e) { echo $e->getMessage(), "\n"; }
var_dump($v->count === 2);
?>
--EXPECT--
bool(true)
bool(true)
__receiverProbe() expects at most 1 argument, 40 given
bool(true)

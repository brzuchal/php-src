--TEST--
Collection intrinsic methods: dynamic method name dispatches (case-insensitive)
--FILE--
<?php
$v = vec[int]{1, 2};
$m = '__receiverProbe';
var_dump($v->$m() === $v);
$m = '__RECEIVERPROBE';        // case-insensitive, like property hooks
var_dump($v->$m() === $v);
$m = 'noSuchMethod';
try { $v->$m(); } catch (\Error $e) { echo $e->getMessage(), "\n"; }
?>
--EXPECT--
bool(true)
bool(true)
Call to undefined method noSuchMethod() on collection

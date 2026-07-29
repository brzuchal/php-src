--TEST--
Collection intrinsic methods: a reference to a collection dispatches like a direct receiver
--FILE--
<?php
$v = vec[int]{1, 2};
$rv =& $v;
var_dump($rv->__receiverProbe() === $v);           // reference to a vec

$s = set[string]{"a", "b"};
$rs =& $s;
var_dump($rs->__receiverProbe() === $s);           // reference to a set

$t = tuple[int, string]{1, "x"};
$rt =& $t;
var_dump($rt->__receiverProbe() === $t);           // reference to a tuple

$c = vec[int]{9};
$r1 =& $c; $r2 =& $r1;
var_dump($r2->__receiverProbe() === $c);           // chained references

$u = vec[int]{5};
$ru =& $u;
try { $ru->nope(); } catch (\Error $e) { echo $e->getMessage(), "\n"; }

// FCC through a reference retains the receiver after the ref target is gone:
$w = vec[int]{7, 8};
$rw =& $w;
$f = $rw->__receiverProbe(...);
unset($w, $rw);
var_dump($f() === $f());
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
Call to undefined method nope() on collection
bool(true)

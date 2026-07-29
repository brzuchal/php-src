--TEST--
Collection intrinsic methods: error paths (arity, named, type, unknown)
--FILE--
<?php
$v = vec[int]{1, 2};
function try_it(callable $fn): void {
    try { $fn(); } catch (\Throwable $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }
}
try_it(fn() => $v->noSuchMethod());
try_it(fn() => $v->__receiverProbe(1, 2));
try_it(fn() => $v->__receiverProbe(nope: 1));
try_it(fn() => $v->__receiverProbe("not-an-int"));
// $v survives every failed call:
var_dump($v->count === 2);
?>
--EXPECTF--
Error: Call to undefined method noSuchMethod() on collection
ArgumentCountError: __receiverProbe() expects at most 1 argument, 2 given
Error: Unknown named parameter $nope
TypeError: __receiverProbe(): Argument #1 ($value) must be of type int, string given%A
bool(true)

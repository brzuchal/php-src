--TEST--
vec: append()/prepend() argument and element-type errors leave the receiver unchanged
--FILE--
<?php
$v = vec[int]{1, 2};
function t(string $l, callable $fn): void {
    try { $fn(); echo "$l: no error\n"; }
    catch (\Throwable $e) { echo "$l: ", get_class($e), ": ", $e->getMessage(), "\n"; }
}
t("wrong type",      fn() => $v->append("x"));
t("wrong type prep", fn() => $v->prepend(1.5));
t("missing arg",     fn() => $v->append());
t("extra arg",       fn() => $v->append(1, 2));
t("unknown named",   fn() => $v->append(nope: 1));

$r = $v->append(value: 9);           // correct named argument
var_dump($r->count === 3);

var_dump($v->count === 2);           // receiver unchanged after every failure
?>
--EXPECTF--
wrong type: TypeError: vec[int]::append(): Argument #1 ($value) must be of type int, string given
wrong type prep: TypeError: vec[int]::prepend(): Argument #1 ($value) must be of type int, float given
missing arg: ArgumentCountError: append() expects exactly 1 argument, 0 given
extra arg: ArgumentCountError: append() expects exactly 1 argument, 2 given
unknown named: Error: Unknown named parameter $nope
bool(true)
bool(true)

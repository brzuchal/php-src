--TEST--
vec: withAt()/withoutAt() argument, range and element-type errors leave the receiver unchanged
--FILE--
<?php
declare(strict_types=1);

$v = vec[int]{10, 20, 30};

function t(string $l, callable $fn): void {
    try { $fn(); echo "$l: no error\n"; }
    catch (\Throwable $e) { echo "$l: ", get_class($e), ": ", $e->getMessage(), "\n"; }
}

// non-int index (strict): TypeError, reported at argument #1 without a receiver prefix
t("wt float idx",  fn() => $v->withAt(1.5, 9));
t("wt bool idx",   fn() => $v->withAt(true, 9));
t("wo array idx",  fn() => $v->withoutAt([1]));
t("wo null idx",   fn() => $v->withoutAt(null));

// out-of-range index: ValueError naming the valid range. PHP_INT_MAX is the
// widest index; its echoed value is 64-bit on LP64 and 32-bit on ILP32, so the
// "huge" line is matched with %d (see --EXPECTF-- below) rather than a literal.
t("wt negative",   fn() => $v->withAt(-1, 9));
t("wt == count",   fn() => $v->withAt(3, 9));
t("wt huge",       fn() => $v->withAt(PHP_INT_MAX, 9));
t("wo negative",   fn() => $v->withoutAt(-1));
t("wo == count",   fn() => $v->withoutAt(3));

// wrong element type: TypeError at argument #2 ($value) for withAt
t("wt bad value",  fn() => $v->withAt(1, "x"));
t("wt bad value2", fn() => $v->withAt(1, 1.5));

// arity
t("wt missing val", fn() => $v->withAt(1));
t("wt no args",     fn() => $v->withAt());
t("wt extra",       fn() => $v->withAt(1, 2, 3));
t("wo no args",     fn() => $v->withoutAt());
t("wo extra",       fn() => $v->withoutAt(1, 2));

// unknown named argument
t("wt unknown named", fn() => $v->withAt(idx: 1, value: 9));

// empty vec has its own wording (no inverted "0 and -1" range)
$empty = vec[int]{5};
$empty = $empty->withoutAt(0);
t("empty wo", fn() => $empty->withoutAt(0));
t("empty wt", fn() => $empty->withAt(0, 1));

// correct named arguments work
var_dump($v->withAt(index: 1, value: 99)->count === 3);
var_dump($v->withoutAt(index: 0)->count === 2);

// receiver unchanged after every failure
var_dump($v->count === 3);
?>
--EXPECTF--
wt float idx: TypeError: withAt(): Argument #1 ($index) must be of type int, float given
wt bool idx: TypeError: withAt(): Argument #1 ($index) must be of type int, true given
wo array idx: TypeError: withoutAt(): Argument #1 ($index) must be of type int, array given
wo null idx: TypeError: withoutAt(): Argument #1 ($index) must be of type int, null given
wt negative: ValueError: vec[int]::withAt(): Argument #1 ($index) must be between 0 and 2, -1 given
wt == count: ValueError: vec[int]::withAt(): Argument #1 ($index) must be between 0 and 2, 3 given
wt huge: ValueError: vec[int]::withAt(): Argument #1 ($index) must be between 0 and 2, %d given
wo negative: ValueError: vec[int]::withoutAt(): Argument #1 ($index) must be between 0 and 2, -1 given
wo == count: ValueError: vec[int]::withoutAt(): Argument #1 ($index) must be between 0 and 2, 3 given
wt bad value: TypeError: vec[int]::withAt(): Argument #2 ($value) must be of type int, string given
wt bad value2: TypeError: vec[int]::withAt(): Argument #2 ($value) must be of type int, float given
wt missing val: ArgumentCountError: withAt() expects exactly 2 arguments, 1 given
wt no args: ArgumentCountError: withAt() expects exactly 2 arguments, 0 given
wt extra: ArgumentCountError: withAt() expects exactly 2 arguments, 3 given
wo no args: ArgumentCountError: withoutAt() expects exactly 1 argument, 0 given
wo extra: ArgumentCountError: withoutAt() expects exactly 1 argument, 2 given
wt unknown named: Error: Unknown named parameter $idx
empty wo: ValueError: vec[int]::withoutAt(): Argument #1 ($index) must be a valid index, but vec[int] is empty
empty wt: ValueError: vec[int]::withAt(): Argument #1 ($index) must be a valid index, but vec[int] is empty
bool(true)
bool(true)
bool(true)

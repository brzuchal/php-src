--TEST--
tuple: withAt() positional type, range and argument errors leave the receiver unchanged
--FILE--
<?php
declare(strict_types=1);

$t = tuple[int, string]{1, "a"};

function t(string $l, callable $fn): void {
    try { $fn(); echo "$l: no error\n"; }
    catch (\Throwable $e) { echo "$l: ", get_class($e), ": ", $e->getMessage(), "\n"; }
}

// wrong replacement type, independently per position: position 0 is int,
// position 1 is string -- the diagnostic names the *selected position's* type
t("pos0 wrong", fn() => $t->withAt(0, "wrong"));
t("pos1 wrong", fn() => $t->withAt(1, 123));
t("pos0 float", fn() => $t->withAt(0, 1.5));

// non-int index (strict): TypeError at argument #1, no receiver prefix
t("float idx",  fn() => $t->withAt(1.5, "b"));
t("bool idx",   fn() => $t->withAt(true, "b"));
t("null idx",   fn() => $t->withAt(null, "b"));

// out-of-range index: ValueError naming the valid range (arity is 2)
t("negative",   fn() => $t->withAt(-1, "b"));
t("== arity",   fn() => $t->withAt(2, "b"));
t("huge",       fn() => $t->withAt(PHP_INT_MAX, "b"));

// arity
t("missing val",fn() => $t->withAt(0));
t("no args",    fn() => $t->withAt());
t("extra",      fn() => $t->withAt(0, 1, 2));

// unknown named argument
t("unknown named", fn() => $t->withAt(idx: 0, value: 2));

// correct named arguments work
var_dump($t->withAt(index: 0, value: 2)->count === 2);
var_dump($t->withAt(index: 1, value: "z")->count === 2);

// receiver unchanged after every failure
var_dump($t->count === 2);
?>
--EXPECT--
pos0 wrong: TypeError: tuple[int,string]::withAt(): Argument #2 ($value) must be of type int, string given
pos1 wrong: TypeError: tuple[int,string]::withAt(): Argument #2 ($value) must be of type string, int given
pos0 float: TypeError: tuple[int,string]::withAt(): Argument #2 ($value) must be of type int, float given
float idx: TypeError: withAt(): Argument #1 ($index) must be of type int, float given
bool idx: TypeError: withAt(): Argument #1 ($index) must be of type int, true given
null idx: TypeError: withAt(): Argument #1 ($index) must be of type int, null given
negative: ValueError: tuple[int,string]::withAt(): Argument #1 ($index) must be between 0 and 1, -1 given
== arity: ValueError: tuple[int,string]::withAt(): Argument #1 ($index) must be between 0 and 1, 2 given
huge: ValueError: tuple[int,string]::withAt(): Argument #1 ($index) must be between 0 and 1, 9223372036854775807 given
missing val: ArgumentCountError: withAt() expects exactly 2 arguments, 1 given
no args: ArgumentCountError: withAt() expects exactly 2 arguments, 0 given
extra: ArgumentCountError: withAt() expects exactly 2 arguments, 3 given
unknown named: Error: Unknown named parameter $idx
bool(true)
bool(true)
bool(true)

--TEST--
set: with()/without() wrong value type and argument errors leave the receiver unchanged
--FILE--
<?php
declare(strict_types=1);

$s = set[int]{1, 2};

function t(string $l, callable $fn): void {
    try { $fn(); echo "$l: no error\n"; }
    catch (\Throwable $e) { echo "$l: ", get_class($e), ": ", $e->getMessage(), "\n"; }
}

// wrong value type for BOTH methods -- a TypeError even though such a value could
// never be a member (the element type gate runs before any membership check)
t("with str",     fn() => $s->with("1"));
t("without str",  fn() => $s->without("1"));
t("with float",   fn() => $s->with(1.0));
t("without float",fn() => $s->without(1.0));
t("with bool",    fn() => $s->with(true));
t("without bool", fn() => $s->without(true));
t("with array",   fn() => $s->with([1]));

// arity
t("with no args", fn() => $s->with());
t("with extra",   fn() => $s->with(1, 2));
t("without no args", fn() => $s->without());
t("without extra",   fn() => $s->without(1, 2));

// unknown named argument
t("unknown named", fn() => $s->with(val: 1));

// correct named argument
var_dump($s->with(value: 3)->count === 3);
var_dump($s->without(value: 1)->count === 1);

// receiver unchanged after every failure
var_dump($s->count === 2);
?>
--EXPECT--
with str: TypeError: set[int]::with(): Argument #1 ($value) must be of type int, string given
without str: TypeError: set[int]::without(): Argument #1 ($value) must be of type int, string given
with float: TypeError: set[int]::with(): Argument #1 ($value) must be of type int, float given
without float: TypeError: set[int]::without(): Argument #1 ($value) must be of type int, float given
with bool: TypeError: set[int]::with(): Argument #1 ($value) must be of type int, true given
without bool: TypeError: set[int]::without(): Argument #1 ($value) must be of type int, true given
with array: TypeError: set[int]::with(): Argument #1 ($value) must be of type int, array given
with no args: ArgumentCountError: with() expects exactly 1 argument, 0 given
with extra: ArgumentCountError: with() expects exactly 1 argument, 2 given
without no args: ArgumentCountError: without() expects exactly 1 argument, 0 given
without extra: ArgumentCountError: without() expects exactly 1 argument, 2 given
unknown named: Error: Unknown named parameter $val
bool(true)
bool(true)
bool(true)

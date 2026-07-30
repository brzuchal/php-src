--TEST--
set: union()/intersect()/diff() require a set of the receiver's exact descriptor
--FILE--
<?php
$a = set[int]{1, 2};

function t(string $l, callable $fn): void {
    try { $fn(); echo "$l: no error\n"; }
    catch (\Throwable $e) { echo "$l: ", get_class($e), ": ", $e->getMessage(), "\n"; }
}

// descriptor mismatch: a set of a different element type is a TypeError (not merged /
// widened / recompared). The message names both full collection types.
t("union set[string]",     fn() => $a->union(set[string]{"x"}));
t("intersect set[string]", fn() => $a->intersect(set[string]{"x"}));
t("diff set[string]",      fn() => $a->diff(set[string]{"x"}));

// wrong kind / non-collection: same message, given type is the argument's actual type
t("union vec",    fn() => $a->union(vec[int]{1}));
t("union tuple",  fn() => $a->union(tuple[int, int]{1, 2}));
t("union array",  fn() => $a->union([1, 2]));
t("union int",    fn() => $a->union(5));
t("union null",   fn() => $a->union(null));

// arity
t("union no args", fn() => $a->union());
t("union extra",   fn() => $a->union(set[int]{1}, set[int]{2}));
t("diff no args",  fn() => $a->diff());

// unknown named argument
t("unknown named", fn() => $a->union(set: set[int]{1}));

// correct named argument
var_dump($a->union(other: set[int]{3})->count === 3);
var_dump($a->intersect(other: set[int]{2})->count === 1);

// receiver unchanged after every failure
var_dump($a->count === 2);
?>
--EXPECT--
union set[string]: TypeError: set[int]::union(): Argument #1 ($other) must be of type set[int], set[string] given
intersect set[string]: TypeError: set[int]::intersect(): Argument #1 ($other) must be of type set[int], set[string] given
diff set[string]: TypeError: set[int]::diff(): Argument #1 ($other) must be of type set[int], set[string] given
union vec: TypeError: set[int]::union(): Argument #1 ($other) must be of type set[int], vec[int] given
union tuple: TypeError: set[int]::union(): Argument #1 ($other) must be of type set[int], tuple[int,int] given
union array: TypeError: set[int]::union(): Argument #1 ($other) must be of type set[int], array given
union int: TypeError: set[int]::union(): Argument #1 ($other) must be of type set[int], int given
union null: TypeError: set[int]::union(): Argument #1 ($other) must be of type set[int], null given
union no args: ArgumentCountError: union() expects exactly 1 argument, 0 given
union extra: ArgumentCountError: union() expects exactly 1 argument, 2 given
diff no args: ArgumentCountError: diff() expects exactly 1 argument, 0 given
unknown named: Error: Unknown named parameter $set
bool(true)
bool(true)
bool(true)

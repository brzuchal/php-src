--TEST--
vec/tuple indexed access is read-only: assign/append/op-assign/inc/unset/by-ref all reject
--FILE--
<?php
$v = vec[int]{10, 20, 30};
$t = tuple[int, string]{1, "a"};

function e(string $l, callable $fn): void {
    try { $fn(); echo "$l: NOT REJECTED\n"; }
    catch (\Throwable $ex) { echo "$l: ", get_class($ex), ": ", $ex->getMessage(), "\n"; }
}

// every mutating form must error; none may modify the immutable collection
e('assign',     function () use ($v) { $v[0] = 99; });
e('append',     function () use ($v) { $v[] = 99; });
e('op-assign',  function () use ($v) { $v[0] += 1; });
e('inc',        function () use ($v) { $v[0]++; });
e('dec',        function () use ($v) { $v[0]--; });
e('unset',      function () use ($v) { unset($v[0]); });
e('by-ref',     function () use ($v) { $r = &$v[0]; });
e('ref-arg',    function () use ($v) { $f = function (&$x) {}; $f($v[0]); });
e('tuple write',function () use ($t) { $t[0] = 9; });
e('nested write', function () use ($v) { $v[0]->prop = 1; });

// the receivers are completely unchanged
var_dump($v[0] === 10, $v->count === 3, $t[0] === 1, $t->count === 2);
?>
--EXPECTF--
assign: Error: Cannot use a scalar value as an array
append: Error: Cannot use a scalar value as an array
op-assign: Error: Cannot use a scalar value as an array
inc: Error: Cannot modify an immutable collection
dec: Error: Cannot modify an immutable collection
unset: Error: Cannot unset offset in a non-array variable
by-ref: Error: Cannot modify an immutable collection
ref-arg: Error: Cannot modify an immutable collection
tuple write: Error: Cannot use a scalar value as an array
nested write: Error: %s
bool(true)
bool(true)
bool(true)
bool(true)

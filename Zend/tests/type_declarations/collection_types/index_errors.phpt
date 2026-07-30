--TEST--
vec/tuple indexed read: strict-int key (TypeError), out-of-range/negative (ValueError), set (Error)
--FILE--
<?php
$v = vec[int]{10, 20, 30};
$t = tuple[int, string]{1, "a"};
$s = set[int]{7, 8};
$empty = vec[int]{};

function e(string $l, callable $fn): void {
    try { $fn(); echo "$l: no error\n"; }
    catch (\Throwable $ex) { echo "$l: ", get_class($ex), ": ", $ex->getMessage(), "\n"; }
}

// non-int key: strict int TYPE (no coercion of "0"/1.0/true/null) -> TypeError
e('string',      fn() => $v["1"]);
e('non-num str', fn() => $v["x"]);
e('float',       fn() => $v[1.0]);
e('float frac',  fn() => $v[1.5]);
e('bool',        fn() => $v[true]);
e('null',        fn() => $v[null]);
e('array',       fn() => $v[[1]]);

// out-of-range / negative on a known-size immutable value -> ValueError (loud, not warning+null)
e('at count',    fn() => $v[3]);
e('past end',    fn() => $v[999]);
e('negative',    fn() => $v[-1]);
e('empty',       fn() => $empty[0]);
e('tuple oob',   fn() => $t[2]);
e('tuple neg',   fn() => $t[-1]);

// set is not positionally indexable
e('set read',    fn() => $s[0]);

// receiver unchanged after every failure
var_dump($v->count === 3, $t->count === 2, $s->count === 2);
?>
--EXPECT--
string: TypeError: Collection index must be of type int, string given
non-num str: TypeError: Collection index must be of type int, string given
float: TypeError: Collection index must be of type int, float given
float frac: TypeError: Collection index must be of type int, float given
bool: TypeError: Collection index must be of type int, true given
null: TypeError: Collection index must be of type int, null given
array: TypeError: Collection index must be of type int, array given
at count: ValueError: Collection index 3 is out of range
past end: ValueError: Collection index 999 is out of range
negative: ValueError: Collection index -1 is out of range
empty: ValueError: Collection index 0 is out of range
tuple oob: ValueError: Collection index 2 is out of range
tuple neg: ValueError: Collection index -1 is out of range
set read: Error: Cannot index a set
bool(true)
bool(true)
bool(true)

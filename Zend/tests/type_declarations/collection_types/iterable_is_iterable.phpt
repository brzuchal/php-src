--TEST--
F2: is_iterable() is true for native collections (vec/set/tuple), false for objects
--FILE--
<?php
var_dump(is_iterable(vec[int]{1, 2, 3}));
var_dump(is_iterable(set[int]{1, 2}));
var_dump(is_iterable(tuple[int, string]{1, "a"}));
var_dump(is_iterable([]));
var_dump(is_iterable(new ArrayIterator([])));
var_dump(is_iterable(new stdClass));
var_dump(is_iterable(42));
var_dump(is_iterable("x"));
echo "ok\n";
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(false)
bool(false)
bool(false)
ok

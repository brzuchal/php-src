--TEST--
Collection intrinsic methods: a rejected argument names a nested member type via the node-aware stringifier (no crash)
--FILE--
<?php
// Regression: the intrinsic-method value-type error used zend_type_to_string() on
// the member type, which for a NESTED collection member (vec[vec[int]], set[vec[int]],
// tuple[vec[int],int]) carries a canonical zend_collection_info node -- misread as a
// compile-time descriptor, it dereferenced bogus fields and crashed. It must instead
// stringify the member through its canonical node and raise a TypeError.

function fail(callable $fn): string {
    try { $fn(); return "NO THROW"; }
    catch (\TypeError $e) { return $e->getMessage(); }
}

echo fail(fn() => (vec[vec[int]]{})->append(1)), "\n";
echo fail(fn() => (vec[vec[int]]{})->prepend(1)), "\n";
echo fail(fn() => (set[vec[int]]{})->with(1)), "\n";
echo fail(fn() => (tuple[vec[int], int]{vec[int]{1}, 2})->withAt(0, 5)), "\n";

// A non-nested member type still reports correctly.
echo fail(fn() => (vec[int]{})->append("x")), "\n";
?>
--EXPECT--
vec[vec[int]]::append(): Argument #1 ($value) must be of type vec[int], int given
vec[vec[int]]::prepend(): Argument #1 ($value) must be of type vec[int], int given
set[vec[int]]::with(): Argument #1 ($value) must be of type vec[int], int given
tuple[vec[int],int]::withAt(): Argument #2 ($value) must be of type vec[int], int given
vec[int]::append(): Argument #1 ($value) must be of type int, string given

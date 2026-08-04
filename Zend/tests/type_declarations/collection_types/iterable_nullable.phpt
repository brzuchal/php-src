--TEST--
F2: ?iterable and iterable|null are the same type and both accept collections; explicit array|Traversable|null rejects them
--FILE--
<?php
// ?iterable, iterable|null and null|iterable are the same type: all accept
// collections and null (the iterable keyword's fallback bit is preserved in the
// union, so it is not decomposed into a bare Traversable|array|null).
function n(?iterable $x): string { return $x === null ? "null" : get_debug_type($x); }
function u(iterable|null $x): string { return $x === null ? "null" : get_debug_type($x); }
function r(null|iterable $x): string { return $x === null ? "null" : get_debug_type($x); }

echo n(vec[int]{1}), "\n";
echo n(null), "\n";
echo u(set[int]{1}), "\n";
echo u(null), "\n";
echo r(tuple[int]{1}), "\n";
echo r(null), "\n";

// An explicitly written array|Traversable|null carries no iterable provenance
// and stays a distinct, collection-free type.
function c(array|Traversable|null $x): string { return $x === null ? "null" : get_debug_type($x); }
echo c([1]), "\n";
echo c(null), "\n";
try {
    c(vec[int]{1});
} catch (\TypeError $e) {
    echo "explicit union rejects collection\n";
}
echo "ok\n";
?>
--EXPECT--
collection
null
collection
null
collection
null
array
null
explicit union rejects collection
ok

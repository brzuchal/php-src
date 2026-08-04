--TEST--
F2: table-driven -- every union carrying the `iterable` keyword accepts collections; explicit concrete unions and non-iterable types reject; Reflection shape/string is identical for provenance and explicit forms
--FILE--
<?php
class Foo {}

// Accepting forms: the `iterable` keyword's provenance is preserved through
// every union shape, so native collections are accepted.
function a1(iterable $x): string { return get_debug_type($x); }
function a2(?iterable $x): string { return $x === null ? "null" : get_debug_type($x); }
function a3(iterable|null $x): string { return $x === null ? "null" : get_debug_type($x); }
function a4(null|iterable $x): string { return $x === null ? "null" : get_debug_type($x); }
function a5(iterable|int $x): string { return is_int($x) ? "int" : get_debug_type($x); }
function a6(iterable|false $x): string { return $x === false ? "false" : get_debug_type($x); }
function a7(iterable|object $x): string { return get_debug_type($x); }
function a8(iterable|Foo $x): string { return get_debug_type($x); }
function a9(iterable|Foo|null $x): string { return $x === null ? "null" : get_debug_type($x); }
function a10(iterable|null|false $x): string { return $x === false ? "false" : ($x === null ? "null" : get_debug_type($x)); }

// Rejecting forms: an explicitly written array|Traversable union has no keyword
// provenance, and object/Traversable never match a collection.
function r1(array|Traversable $x): string { return get_debug_type($x); }
function r2(array|Traversable|null $x): string { return get_debug_type($x); }
function r3(array|Traversable|int $x): string { return get_debug_type($x); }
function r4(array|Traversable|Foo|null $x): string { return get_debug_type($x); }
function r5(object $x): string { return get_debug_type($x); }
function r6(Traversable $x): string { return get_debug_type($x); }

$c = vec[int]{1, 2, 3};

echo "== acceptance ==\n";
foreach (['a1','a2','a3','a4','a5','a6','a7','a8','a9','a10'] as $f) {
    try { printf("%-4s accept => %s\n", $f, $f($c)); }
    catch (\TypeError $e) { printf("%-4s accept => TYPEERROR\n", $f); }
}
foreach (['r1','r2','r3','r4','r5','r6'] as $f) {
    try { printf("%-4s reject => %s\n", $f, $f($c)); }
    catch (\TypeError $e) { printf("%-4s reject => TYPEERROR\n", $f); }
}

echo "\n== reflection (provenance is invisible: a3 and r2 render identically) ==\n";
foreach (['a1','a2','a3','a4','a5','a6','a7','a8','a9','a10','r1','r2','r3','r4','r5','r6'] as $f) {
    $t = (new ReflectionFunction($f))->getParameters()[0]->getType();
    printf("%-4s %-20s %s\n", $f, get_class($t), (string)$t);
}
?>
--EXPECT--
== acceptance ==
a1   accept => collection
a2   accept => collection
a3   accept => collection
a4   accept => collection
a5   accept => collection
a6   accept => collection
a7   accept => collection
a8   accept => collection
a9   accept => collection
a10  accept => collection
r1   reject => TYPEERROR
r2   reject => TYPEERROR
r3   reject => TYPEERROR
r4   reject => TYPEERROR
r5   reject => TYPEERROR
r6   reject => TYPEERROR

== reflection (provenance is invisible: a3 and r2 render identically) ==
a1   ReflectionNamedType  iterable
a2   ReflectionNamedType  ?iterable
a3   ReflectionUnionType  Traversable|array|null
a4   ReflectionUnionType  Traversable|array|null
a5   ReflectionUnionType  Traversable|array|int
a6   ReflectionUnionType  Traversable|array|false
a7   ReflectionUnionType  Traversable|object|array
a8   ReflectionUnionType  Traversable|Foo|array
a9   ReflectionUnionType  Traversable|Foo|array|null
a10  ReflectionUnionType  Traversable|array|false|null
r1   ReflectionUnionType  Traversable|array
r2   ReflectionUnionType  Traversable|array|null
r3   ReflectionUnionType  Traversable|array|int
r4   ReflectionUnionType  Traversable|Foo|array|null
r5   ReflectionNamedType  object
r6   ReflectionNamedType  Traversable

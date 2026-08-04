--TEST--
ReflectionParameter::getClass(): iterable-provenance types report Traversable; collection descriptors do not
--FILE--
<?php
// getClass() is deprecated since 8.0; this test is only about which class it reports.
error_reporting(E_ALL & ~E_DEPRECATED);

function name(ReflectionParameter $p): string {
    $c = $p->getClass();
    return $c ? $c->getName() : 'NULL';
}
function param(string|Closure $fn): ReflectionParameter {
    return (new ReflectionFunction($fn))->getParameters()[0];
}

// (1) A genuine Traversable|array union, and the decomposed `iterable` form that an
// internal function's argument carries (Traversable|array + iterable provenance bit),
// both report Traversable -- the latter is the regression this fixes.
echo "stub iterable:    ", name(param('iterator_to_array')), "\n";
echo "src Trav|array:   ", name(param(function (Traversable|array $x) {})), "\n";

// (3) Ordinary iterable behaviour is unchanged.
echo "src iterable:     ", name(param(function (iterable $x) {})), "\n";
echo "src Traversable:  ", name(param(function (Traversable $x) {})), "\n";

// (4) Nullable and intersection cases already supported by Reflection are unchanged:
// nullable iterable still reports Traversable; an intersection has no single class.
echo "src ?iterable:    ", name(param(function (?iterable $x) {})), "\n";
echo "intersection:     ", name(param(function (Countable&Traversable $x) {})), "\n";

// (2) Collection descriptor types are NOT iterable-fallback classes -> NULL, and the
// parameter still has a genuine (collection) reflection type.
echo "collection vec:   ", name(param(function (vec[int] $x) {})), "\n";
echo "collection set:   ", name(param(function (set[string] $x) {})), "\n";
echo "collection type:  ", get_class(param(function (vec[int] $x) {})->getType()), "\n";

// Plain array / scalar: NULL (unchanged).
echo "array:            ", name(param(function (array $x) {})), "\n";
echo "int:              ", name(param(function (int $x) {})), "\n";
?>
--EXPECT--
stub iterable:    Traversable
src Trav|array:   Traversable
src iterable:     Traversable
src Traversable:  Traversable
src ?iterable:    Traversable
intersection:     NULL
collection vec:   NULL
collection set:   NULL
collection type:  ReflectionCollectionType
array:            NULL
int:              NULL

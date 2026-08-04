--TEST--
F2: iterable accepts collections, but an explicit array|Traversable union still rejects them
--FILE--
<?php
function itr(iterable $x): string { return "accepted"; }
function at(array|Traversable $x): string { return "accepted"; }

echo itr(vec[int]{1, 2}), "\n";          // iterable accepts
echo at([1, 2]), "\n";                    // array ok
echo at(new ArrayIterator([])), "\n";     // Traversable ok
try {
    at(vec[int]{1, 2});                   // collection rejected
} catch (\TypeError $e) {
    echo $e->getMessage(), "\n";
}
echo "ok\n";
?>
--EXPECTF--
accepted
accepted
accepted
at(): Argument #1 ($x) must be of type Traversable|array, vec[int] given, called in %s on line %d
ok

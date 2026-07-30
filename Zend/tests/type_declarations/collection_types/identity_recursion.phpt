--TEST--
collection identity: a cycle through an array-with-reference throws cleanly, never overflows
--FILE--
<?php
// A collection cannot self-reference (immutable, built bottom-up), but a mutable array
// element carrying a reference can close a loop back to a collection. Comparing two such
// values must reuse the array recursion guard (throw "Nesting level too deep"), not smash
// the stack.
$a1 = [1]; $arr1 = [&$a1]; $v1 = vec[array]{$arr1}; $a1[0] = $v1;
$a2 = [1]; $arr2 = [&$a2]; $v2 = vec[array]{$arr2}; $a2[0] = $v2;

// Reflexive: the same-pointer fast path never descends, so no recursion.
var_dump($v1 === $v1);   // true

try {
    var_dump($v1 === $v2);
    echo "NOT THROWN\n";
} catch (\Error $e) {
    echo $e->getMessage(), "\n";
}
echo "process survived\n";

// The same guard protects array-only recursion; the collection layer adds no new hazard.
$c1 = [1]; $c1[0] = &$c1;
$c2 = [1]; $c2[0] = &$c2;
try {
    var_dump($c1 === $c2);
} catch (\Error $e) {
    echo $e->getMessage(), "\n";
}
echo "done\n";
?>
--EXPECT--
bool(true)
Nesting level too deep - recursive dependency?
process survived
Nesting level too deep - recursive dependency?
done

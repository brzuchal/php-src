--TEST--
Bare tuple[]: arity erasure -- accepts a tuple of any arity
--FILE--
<?php
function acceptTuple(tuple[] $t): int { return $t->count; }

echo acceptTuple(tuple[int]{1}), "\n";                        // 1
echo acceptTuple(tuple[int, string]{1, "a"}), "\n";           // 2
echo acceptTuple(tuple[int, string, bool]{1, "a", true}), "\n"; // 3

try { acceptTuple(vec[int]{1}); }
catch (\TypeError $e) { echo $e->getMessage(), "\n"; }
?>
--EXPECTF--
1
2
3
acceptTuple(): Argument #1 ($t) must be of type tuple[], vec[int] given, called in %s on line %d

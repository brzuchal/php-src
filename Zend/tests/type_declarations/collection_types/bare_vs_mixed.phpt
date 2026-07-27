--TEST--
Bare vec[] is distinct from vec[mixed], and is a type (not an "empty vec")
--FILE--
<?php
function bare(vec[] $v): string { return "bare-ok"; }
function mixed(vec[mixed] $v): string { return "mixed-ok"; }

// vec[] accepts a concrete vec[int]; vec[mixed] rejects it (concrete is invariant)
echo bare(vec[int]{1, 2}), "\n";
try { mixed(vec[int]{1, 2}); }
catch (\TypeError $e) { echo $e->getMessage(), "\n"; }

// vec[] is a TYPE, not "empty vec": an empty vec VALUE is still a vec
echo bare(vec[int]{}), "\n";

// the two declarations stringify differently
$a = (new ReflectionFunction('bare'))->getParameters()[0]->getType();
echo (string) $a, "\n";
?>
--EXPECTF--
bare-ok
mixed(): Argument #1 ($v) must be of type vec[mixed], vec[int] given, called in %s on line %d
bare-ok
vec[]

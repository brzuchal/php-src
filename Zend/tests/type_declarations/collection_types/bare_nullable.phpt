--TEST--
Bare collection-kind types: nullable ?vec[] / ?set[] / ?tuple[]
--FILE--
<?php
function f(?vec[] $v): string { return $v === null ? "null" : "vec"; }

echo f(vec[int]{1}), "\n";          // vec
echo f(null), "\n";                 // null
try { f(set[int]{1}); }             // wrong kind still rejected
catch (\TypeError $e) { echo $e->getMessage(), "\n"; }

$g = function(?tuple[] $t): void {};
$g(tuple[int]{1});
$g(null);
echo "ok\n";
?>
--EXPECTF--
vec
null
f(): Argument #1 ($v) must be of type ?vec[], set[int] given, called in %s on line %d
ok

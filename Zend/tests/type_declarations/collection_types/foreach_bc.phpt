--TEST--
Collection foreach: arrays, objects and generators unaffected; vec/set/tuple stay ordinary identifiers
--FILE--
<?php
$out = [];
foreach ([1, 2, 3] as $k => $v) { $out[] = "$k$v"; }
echo implode("", $out), "\n";

$out = [];
foreach ((object)["a" => 1, "b" => 2] as $k => $v) { $out[] = "$k$v"; }
echo implode("", $out), "\n";

function gen() { yield "x" => 1; yield "y" => 2; }
$out = [];
foreach (gen() as $k => $v) { $out[] = "$k$v"; }
echo implode("", $out), "\n";

// vec/set/tuple are not reserved words
function vec($a) { return $a + 1; }
$set = 5;
const tuple = 7;
echo vec(10), " ", $set, " ", tuple, "\n";
?>
--EXPECT--
011223
a1b2
x1y2
11 5 7

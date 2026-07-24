--TEST--
Collection foreach: source stays alive when unset mid-loop and for temporary sources
--FILE--
<?php
$c = vec[int]{1, 2, 3};
$out = "";
foreach ($c as $v) { unset($c); $out .= $v; }
echo "unset: $out\n";

$out = "";
foreach (vec[int]{4, 5, 6} as $v) { $out .= $v; }
echo "temp-literal: $out\n";

function mk(): vec[int] { return vec[int]{7, 8}; }
$out = "";
foreach (mk() as $v) { $out .= $v; }
echo "temp-call: $out\n";
?>
--EXPECT--
unset: 123
temp-literal: 456
temp-call: 78

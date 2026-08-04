--TEST--
Collection foreach: break, continue, return and exception unwind clean up correctly
--FILE--
<?php
$out = "";
foreach (vec[int]{1, 2, 3, 4} as $v) { if ($v === 3) break; $out .= $v; }
echo "break: $out\n";

$out = "";
foreach (vec[int]{1, 2, 3, 4} as $v) { if ($v % 2 === 0) continue; $out .= $v; }
echo "continue: $out\n";

function firstEven(): ?int {
    foreach (vec[int]{1, 3, 4, 6} as $v) { if ($v % 2 === 0) return $v; }
    return null;
}
echo "return: ", firstEven(), "\n";

// exception unwinds cleanly and the collection is still usable afterward
$c = vec[int]{1, 2, 3};
try {
    foreach ($c as $v) { if ($v === 2) throw new RuntimeException("stop"); }
} catch (RuntimeException $e) {
    echo "caught: ", $e->getMessage(), "\n";
}
$out = "";
foreach ($c as $v) { $out .= $v; }
echo "after: $out\n";
?>
--EXPECT--
break: 12
continue: 13
return: 4
caught: stop
after: 123

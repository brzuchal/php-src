--TEST--
Collection foreach: empty collections run zero iterations and clean up
--FILE--
<?php
$n = 0;
foreach (vec[int]{} as $v) { $n++; }
foreach (set[int]{} as $v) { $n++; }
echo "iterations: $n\n";
echo "done\n";
?>
--EXPECT--
iterations: 0
done

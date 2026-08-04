--TEST--
Collection foreach: tuple iterates positional order with integer keys
--FILE--
<?php
$t = tuple[int, string, bool]{1, "a", true};
$pairs = [];
foreach ($t as $k => $v) { $pairs[] = "$k:" . var_export($v, true); }
echo implode(" ", $pairs), "\n";
?>
--EXPECT--
0:1 1:'a' 2:true

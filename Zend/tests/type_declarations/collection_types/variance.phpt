--TEST--
collection types: compatibility is invariant
--FILE--
<?php
class P { public function m(vec[int] $a): vec[int] {} }
class Q extends P { public function m(vec[int] $a): vec[int] {} }
echo "identical override accepted\n";
?>
--EXPECT--
identical override accepted

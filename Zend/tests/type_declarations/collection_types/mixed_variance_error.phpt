--TEST--
collection types: a mixed return may not widen a collection prototype
--FILE--
<?php
class P { public function m(): vec[int] {} }
class Q extends P { public function m(): mixed {} }
?>
--EXPECTF--
Fatal error: Declaration of Q::m(): mixed must be compatible with P::m(): vec[int] in %s on line %d

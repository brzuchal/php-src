--TEST--
Bare collection-kind types: cannot be part of a union type
--FILE--
<?php
function u(vec[]|int $x): void {}
?>
--EXPECTF--
Fatal error: Collection type cannot be part of a union type; write ?vec[...] for a nullable collection in %s on line %d

--TEST--
Bare collection-kind types: cannot be part of an intersection type
--FILE--
<?php
interface I {}
function u(vec[]&I $x): void {}
?>
--EXPECTF--
Fatal error: Collection type cannot be part of an intersection type in %s on line %d

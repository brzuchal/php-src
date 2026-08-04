--TEST--
collection types: not allowed inside an intersection type
--FILE--
<?php
function f(vec[int]&Countable $x) {}
?>
--EXPECTF--
Fatal error: Collection type cannot be part of an intersection type in %s on line %d

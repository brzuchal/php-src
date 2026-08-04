--TEST--
collection types: a qualified head is not a collection head
--FILE--
<?php
/* The scanner emits one T_NAME_QUALIFIED token, so `vec[` never appears and no
 * collection head is recognised. With the soft `name '[' args ']'` production
 * dropped, this is a syntax error rather than a compile error. */
function f(\Ns\vec[int] $x) {}
?>
--EXPECTF--
Parse error: syntax error, unexpected token "[", expecting variable in %s on line %d

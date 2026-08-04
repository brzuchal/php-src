--TEST--
collection types: the head must be adjacent to '['
--FILE--
<?php
/* Compound heads are recognised only when the name is immediately followed by
 * '['. Any trivia between them means no head is recognised at all. This is a
 * deliberate restriction pending the RFC; see
 * implementation-notes/parser-architecture-decision.md. */
function f(vec [int] $x) {}
?>
--EXPECTF--
Parse error: syntax error, unexpected token "[", expecting variable in %s on line %d

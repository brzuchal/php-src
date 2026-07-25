--TEST--
Collection intrinsic properties: unknown property read is an Error (closed set)
--FILE--
<?php
$v = vec[int]{1, 2, 3};

// direct unknown
try { $v->nope; }
catch (\Error $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }

// dynamic unknown
try { $n = 'alsoNope'; $v->$n; }
catch (\Error $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }

// intrinsic names are case-sensitive: these are unknown, not count/isEmpty
try { $v->Count; }
catch (\Error $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }
try { $v->ISEMPTY; }
catch (\Error $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }
?>
--EXPECT--
Error: Undefined intrinsic property "nope" on collection
Error: Undefined intrinsic property "alsoNope" on collection
Error: Undefined intrinsic property "Count" on collection
Error: Undefined intrinsic property "ISEMPTY" on collection

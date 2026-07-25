--TEST--
Collection intrinsic properties: nullsafe access
--FILE--
<?php
$v = vec[int]{1, 2, 3};
$n = null;

var_dump($v?->count);        // 3
var_dump($v?->isEmpty);      // false
var_dump($n?->count);        // NULL (short-circuit on null base)
var_dump($n?->isEmpty);      // NULL

// nullsafe on a non-null collection is a normal read: unknown still throws
try {
    $v?->unknown;
} catch (\Error $e) {
    echo $e->getMessage(), "\n";
}
?>
--EXPECT--
int(3)
bool(false)
NULL
NULL
Undefined intrinsic property "unknown" on collection

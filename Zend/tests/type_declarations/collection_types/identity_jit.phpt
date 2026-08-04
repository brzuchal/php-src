--TEST--
collection identity under JIT: IS_IDENTICAL routes to zend_is_identical (function + tracing), typed and untyped
--EXTENSIONS--
opcache
--INI--
opcache.enable=1
opcache.enable_cli=1
opcache.jit_buffer_size=64M
opcache.jit=1235
opcache.jit_hot_func=1
opcache.jit_hot_loop=1
--FILE--
<?php
// The JIT emits a native scalar fast path only for exactly-LONG/exactly-DOUBLE operands;
// a collection operand is MAY_BE_ANY-broad, so === falls through to a zend_is_identical
// call. Verify the hot path agrees with the VM for typed and untyped operands.
function eqTyped(vec[int] $a, vec[int] $b): bool { return $a === $b; }
function eqUntyped($a, $b): bool { return $a === $b; }
function neUntyped($a, $b): bool { return $a !== $b; }

$x = vec[int]{1, 2};
$y = vec[int]{1, 2};   // value-equal, separate allocation
$z = vec[int]{2, 1};   // order differs
$s1 = set[int]{1, 2, 3};
$s2 = set[int]{3, 2, 1};

$eq = 0; $ne = 0; $ord = 0; $setEq = 0;
for ($i = 0; $i < 100000; $i++) {
    if (eqTyped($x, $y))    $eq++;
    if (neUntyped($x, $z))  $ne++;
    if (eqUntyped($x, $z))  $ord++;
    if (eqUntyped($s1, $s2)) $setEq++;
}
var_dump($eq === 100000);      // all equal
var_dump($ne === 100000);      // all not-identical (order differs)
var_dump($ord === 0);          // never identical (order differs)
var_dump($setEq === 100000);   // set order-insensitive: always equal

// correctness after warm-up
var_dump(eqTyped($x, $y), eqUntyped($x, $x), eqUntyped($x, $z));
var_dump(eqUntyped(vec[int]{1}, 5), eqUntyped(vec[int]{1}, null)); // vs non-collection
echo "OK\n";
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(false)
bool(false)
bool(false)
OK

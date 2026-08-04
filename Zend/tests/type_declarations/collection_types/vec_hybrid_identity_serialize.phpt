--TEST--
vec hybrid: identity / serialize / display are representation-independent (flat vs hybrid)
--EXTENSIONS--
zend_test
--FILE--
<?php
function dump(mixed $x): string { ob_start(); var_dump($x); return ob_get_clean(); }

zend_test_make_vec([1, 2, 3], 'int', $flat);              // flat [1,2,3]
zend_test_make_vec([1, 2], 'int', $b1);  $hyb   = $b1->append(3);  // hybrid [1,2 | 3]
zend_test_make_vec([1, 2], 'int', $b2);  $hyb_b = $b2->append(3);  // another hybrid, same value
zend_test_make_vec([1, 2, 4], 'int', $diff);              // different value

// Strict identity, both directions, across representations.
var_dump($flat === $hyb);        // true  (flat === hybrid)
var_dump($hyb === $flat);        // true  (hybrid === flat)
var_dump($hyb === $hyb_b);       // true  (two hybrids, equal value)
var_dump($flat === $diff);       // false (different value)
var_dump($hyb !== $diff);        // true

// Serialize is byte-identical to the equivalent flat value; unserialize round-trips.
var_dump(serialize($flat) === serialize($hyb));    // true
var_dump(unserialize(serialize($hyb)) === $flat);  // true

// Display renders identically for a flat and a hybrid with the same value.
var_dump(dump($flat) === dump($hyb));                          // true (var_dump)
var_dump(print_r($flat, true) === print_r($hyb, true));        // true (print_r)
var_dump(var_export($flat, true) === var_export($hyb, true));  // true (var_export)

// A read-only operation must not mutate/replace the representation: after all the
// reads above, the branch is still a hybrid sharing its base (memory stays flat).
$before = memory_get_usage();
serialize($hyb); dump($hyb); $hyb === $flat;
var_dump(memory_get_usage() - $before < 512);   // true: no flatten of the shared value
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(false)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)

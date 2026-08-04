--TEST--
collection identity: elements use existing strict rules (objects by handle, arrays by value, NAN, scalars)
--FILE--
<?php
// Objects: by handle, not by contents (same rule as ===/spl_object_id).
$o = new stdClass; $o->n = 1;
$p = new stdClass; $p->n = 1;              // equal-looking, different handle
var_dump(vec[stdClass]{$o} === vec[stdClass]{$o});   // true: same handle
var_dump(vec[stdClass]{$o} === vec[stdClass]{$p});   // false: different handle
var_dump(vec[stdClass]{$o, $p} === vec[stdClass]{$o, $p}); // true

// Arrays: by value, recursively (existing strict array identity).
var_dump(vec[array]{[1, [2, 3]]} === vec[array]{[1, [2, 3]]});   // true
var_dump(vec[array]{[1, 2]} === vec[array]{[2, 1]});             // false: order
var_dump(vec[array]{['a' => 1]} === vec[array]{['a' => 1]});     // true
var_dump(vec[array]{['a' => 1, 'b' => 2]} === vec[array]{['b' => 2, 'a' => 1]}); // false: key order (strict array identity)

// NAN matches the scalar strict comparator: NAN !== NAN, but a value is still === itself.
$nan = vec[float]{NAN};
var_dump($nan === $nan);                          // true: reflexive (pointer short-circuit)
var_dump(vec[float]{NAN} === vec[float]{NAN});    // false: element NAN !== NAN
var_dump(vec[float]{0.0} === vec[float]{-0.0});   // true: 0.0 === -0.0, like scalars

// Booleans, ints, strings, floats compared by the usual strict rules.
var_dump(vec[bool]{true, false} === vec[bool]{true, false});   // true
var_dump(vec[bool]{true} === vec[bool]{false});                // false
var_dump(vec[int]{1} === vec[int]{1});                         // true
var_dump(tuple[int, float]{1, 2.5} === tuple[int, float]{1, 2.5}); // true
var_dump(vec[string]{'x'} === vec[string]{'x'});               // true
?>
--EXPECT--
bool(true)
bool(false)
bool(true)
bool(true)
bool(false)
bool(true)
bool(false)
bool(true)
bool(false)
bool(true)
bool(true)
bool(false)
bool(true)
bool(true)
bool(true)

--TEST--
Collection intrinsic properties: ordinary object/enum/scalar property behaviour unchanged
--FILE--
<?php
// ordinary (dynamic) object property named like an intrinsic
$o = new stdClass; $o->count = 7; $o->isEmpty = 'x';
var_dump($o->count, $o->isEmpty);

// readonly object property still throws the ordinary readonly error
class P { public function __construct(public readonly int $count) {} }
$p = new P(5);
var_dump($p->count);
try { $p->count = 9; } catch (\Error $e) { echo "readonly obj: ", $e->getMessage(), "\n"; }

// enum ->name and backed enum ->value are unchanged (real object properties)
enum Suit: string { case Hearts = 'H'; }
var_dump(Suit::Hearts->name, Suit::Hearts->value);

// undefined object property is still a Warning + null, not an Error
$w = new stdClass;
var_dump(@$w->missing);

// scalar and array property reads keep the ordinary non-object warning + null
set_error_handler(function($n, $s) { echo "WARN: $s\n"; return true; });
$i = 5;      $a = $i->count;
$arr = [1];  $b = $arr->count;
restore_error_handler();
var_dump($a, $b);
echo "ok\n";
?>
--EXPECT--
int(7)
string(1) "x"
int(5)
readonly obj: Cannot modify readonly property P::$count
string(6) "Hearts"
string(1) "H"
NULL
WARN: Attempt to read property "count" on int
WARN: Attempt to read property "count" on array
NULL
NULL
ok

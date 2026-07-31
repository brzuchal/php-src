--TEST--
tuple literals build the payload directly (no intermediate array); positional validation, order, cleanup
--EXTENSIONS--
zend_test
--FILE--
<?php
// tuple reuses the vec builder opcodes (INIT_COLLECTION / ADD_COLLECTION_ELEMENT /
// FINISH_COLLECTION) with positional validation: FINISH checks slot i against member i.
// The observable contract (evaluate all, then validate; exact cleanup on every failure) is
// identical to the former array-backed pipeline.

function ev(string $tag, $v) { echo "eval $tag\n"; return $v; }

echo "-- values, arity, positional types --\n";
$t = tuple[int, string, float]{ ev('0', 1), ev('1', 'a'), ev('2', 2.5) };
var_dump($t->count, $t[0], $t[1], $t[2]);

echo "-- arity 1 and a larger literal --\n";
var_dump((tuple[int]{ 7 })[0]);
$big = tuple[int, int, int, int, string, bool]{ 1, 2, 3, 4, 'x', true };
var_dump($big->count, $big[4], $big[5]);

echo "-- wrong type in slot 0, side effect slot 1: all evaluate, error names slot 0 --\n";
try { $x = tuple[int, string]{ ev('0=wrong', 's'), ev('1=side', 2) }; }
catch (\TypeError $e) { echo $e->getMessage(), "\n"; }

echo "-- wrong type in a LATE slot: validated against member i --\n";
try { $x = tuple[int, int, string]{ ev('0', 1), ev('1', 2), ev('2=wrong', 99) }; }
catch (\TypeError $e) { echo $e->getMessage(), "\n"; }

echo "-- thrown exception mid-list stops later evaluation; partial payload freed --\n";
function boom(): int { echo "boom\n"; throw new RuntimeException('mid'); }
try { $x = tuple[int, int, int]{ ev('0', 1), boom(), ev('2', 3) }; }
catch (\RuntimeException $e) { echo 'caught: ', $e->getMessage(), "\n"; }

echo "-- non-constructible member: elements evaluate first, then 'Cannot create' --\n";
try { $x = tuple[int, ?string]{ ev('0', 1), ev('1', 'a') }; }
catch (\TypeError $e) { echo $e->getMessage(), "\n"; }

echo "-- refcounted + nested + object positional members --\n";
function s(int $i): string { return "v$i"; }
$o = new stdClass; $o->n = 9;
$t = tuple[vec[int], stdClass, string]{ vec[int]{1, 2}, $o, s(3) };
var_dump($t[0][1], $t[1] === $o, $t[2]);

echo "-- value identity of directly-built tuples --\n";
var_dump(tuple[int, string]{1, 'a'} === tuple[int, string]{1, 'a'});
var_dump(tuple[int, string]{1, 'a'} === tuple[int, string]{2, 'a'});

echo "-- vec and set construct correctly (same builder) --\n";
var_dump((vec[int]{1, 2, 3})->count, (set[int]{1, 1, 2})->count);
?>
--EXPECT--
-- values, arity, positional types --
eval 0
eval 1
eval 2
int(3)
int(1)
string(1) "a"
float(2.5)
-- arity 1 and a larger literal --
int(7)
int(6)
string(1) "x"
bool(true)
-- wrong type in slot 0, side effect slot 1: all evaluate, error names slot 0 --
eval 0=wrong
eval 1=side
Element 0 of tuple[int,string] must be of type int, string given
-- wrong type in a LATE slot: validated against member i --
eval 0
eval 1
eval 2=wrong
Element 2 of tuple[int,int,string] must be of type string, int given
-- thrown exception mid-list stops later evaluation; partial payload freed --
eval 0
boom
caught: mid
-- non-constructible member: elements evaluate first, then 'Cannot create' --
eval 0
eval 1
Cannot create a value of type tuple[int,?string]
-- refcounted + nested + object positional members --
int(2)
bool(true)
string(2) "v3"
-- value identity of directly-built tuples --
bool(true)
bool(false)
-- vec and set construct correctly (same builder) --
int(3)
int(2)

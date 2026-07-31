--TEST--
set literals build and deduplicate the payload directly (no intermediate array)
--EXTENSIONS--
zend_test
--FILE--
<?php
// A set[...] literal compiles to INIT_COLLECTION / ADD_COLLECTION_ELEMENT / FINISH_COLLECTION,
// the same direct builder as vec and tuple. Every element expression is stored into an
// exact-size payload; FINISH validates every slot, then deduplicates in place -- keeping the
// first occurrence of each distinct value in first-occurrence order and lowering the published
// count below the allocated capacity. The observable contract is byte-identical to the former
// array-backed pipeline: same evaluation order, same silent dedup, same value identity.

function ev(string $tag, $v) { echo "eval $tag\n"; return $v; }
function order($s): array { $o = []; foreach ($s as $x) { $o[] = is_object($x) ? '#'.spl_object_id($x) : $x; } return $o; }

echo "-- silent dedup: count is the unique cardinality, not the element count --\n";
var_dump((set[int]{1, 1, 2})->count);          // 2
var_dump((set[int]{})->count);                 // 0
var_dump((set[int]{7, 7, 7, 7})->count);       // 1

echo "-- first occurrence kept, first-occurrence order preserved --\n";
var_dump(order(set[int]{3, 1, 2, 1, 3, 2, 1}));       // [3,1,2]
var_dump(order(set[string]{"z", "a", "z", "m", "a"})); // [z,a,m]

echo "-- every element expression runs before any duplicate is eliminated --\n";
$s = set[int]{ ev('A', 1), ev('B-dup', 1), ev('C', 2), ev('D-dup', 1) };
var_dump($s->count);                           // 2

echo "-- identity per element type: NAN never equals NAN; objects by handle; arrays by value --\n";
var_dump((set[float]{NAN, NAN})->count);       // 2
var_dump((set[float]{0.0, -0.0})->count);      // 1
$o = new stdClass;
var_dump((set[stdClass]{$o, $o})->count);      // 1  (same handle)
var_dump((set[stdClass]{new stdClass, new stdClass})->count); // 2 (different handles)
var_dump((set[array]{[1, 2], [1, 2]})->count); // 1  (equal by value)
var_dump((set[array]{[1], [2]})->count);       // 2
var_dump((set[vec[int]]{ vec[int]{1, 2}, vec[int]{1, 2} })->count); // 1 (recursive value identity)
var_dump((set[vec[int]]{ vec[int]{1}, vec[int]{2} })->count);       // 2

echo "-- validate every slot before dedup; the first wrong slot names the source position --\n";
try { $x = set[int]{ ev('0-wrong', 'x'), ev('1', 1) }; }
catch (\TypeError $e) { echo $e->getMessage(), "\n"; }   // Element 0 ...
try { $x = set[int]{ ev('0', 1), ev('1-dup', 1), ev('2-wrong', 'x') }; }
catch (\TypeError $e) { echo $e->getMessage(), "\n"; }   // Element 2 ... (a dup earlier does not shift it)

echo "-- non-constructible set: elements evaluate first, then 'Cannot create' --\n";
try { $x = set[int|string]{ ev('0', 1), ev('1', 2) }; }
catch (\TypeError $e) { echo $e->getMessage(), "\n"; }

echo "-- a thrown expression mid-list stops later evaluation and frees the partial payload --\n";
function boom(): int { echo "boom\n"; throw new RuntimeException('mid'); }
try { $x = set[int]{ ev('0', 1), ev('1-dup', 1), boom(), ev('3', 3) }; }
catch (\RuntimeException $e) { echo 'caught: ', $e->getMessage(), "\n"; }

echo "-- value identity: a dedup-built set equals its already-unique form (slack is invisible) --\n";
var_dump(set[int]{1, 1, 2} === set[int]{1, 2});
var_dump(set[int]{3, 1, 3, 2, 1} === set[int]{3, 1, 2});
var_dump(set[int]{1, 2, 3} === set[int]{3, 2, 1});   // order-insensitive: true

echo "-- discarded duplicates share the kept element's references: no __destruct at construction --\n";
class Tracked {
    public function __construct(public int $id) {}
    public function __destruct() { echo "destruct #$this->id\n"; }
}
$t = new Tracked(1);
$d = set[Tracked]{ $t, $t, $t };               // 2 discards, all the same handle as the kept one
var_dump($d->count);                           // 1
echo "still alive after construction\n";       // no destruct yet: $t and the set both hold #1
unset($t, $d);                                 // now #1 dies exactly once
echo "after unset\n";

echo "-- dup-heavy with refcounted (array/nested) discards frees cleanly --\n";
$big = eval('return set[int]{' . implode(',', array_fill(0, 400, 42)) . '};');
var_dump($big->count);                         // 1
$mixed = set[array]{ [1, 2, 3], [1, 2, 3], [9], [1, 2, 3], [9] };
var_dump($mixed->count);                       // 2
$nested = set[vec[int]]{ vec[int]{1,1}, vec[int]{1,1}, vec[int]{2}, vec[int]{1,1} };
var_dump($nested->count);                      // 2
echo "OK\n";
?>
--EXPECT--
-- silent dedup: count is the unique cardinality, not the element count --
int(2)
int(0)
int(1)
-- first occurrence kept, first-occurrence order preserved --
array(3) {
  [0]=>
  int(3)
  [1]=>
  int(1)
  [2]=>
  int(2)
}
array(3) {
  [0]=>
  string(1) "z"
  [1]=>
  string(1) "a"
  [2]=>
  string(1) "m"
}
-- every element expression runs before any duplicate is eliminated --
eval A
eval B-dup
eval C
eval D-dup
int(2)
-- identity per element type: NAN never equals NAN; objects by handle; arrays by value --
int(2)
int(1)
int(1)
int(2)
int(1)
int(2)
int(1)
int(2)
-- validate every slot before dedup; the first wrong slot names the source position --
eval 0-wrong
eval 1
Element 0 of set[int] must be of type int, string given
eval 0
eval 1-dup
eval 2-wrong
Element 2 of set[int] must be of type int, string given
-- non-constructible set: elements evaluate first, then 'Cannot create' --
eval 0
eval 1
Cannot create a value of type set[string|int]
-- a thrown expression mid-list stops later evaluation and frees the partial payload --
eval 0
eval 1-dup
boom
caught: mid
-- value identity: a dedup-built set equals its already-unique form (slack is invisible) --
bool(true)
bool(true)
bool(true)
-- discarded duplicates share the kept element's references: no __destruct at construction --
int(1)
still alive after construction
destruct #1
after unset
-- dup-heavy with refcounted (array/nested) discards frees cleanly --
int(1)
int(2)
int(2)
OK

--TEST--
vec literals build the payload directly (no intermediate array); order and cleanup preserved
--EXTENSIONS--
zend_test
--FILE--
<?php
// A vec[...] literal compiles to INIT_COLLECTION / ADD_COLLECTION_ELEMENT / FINISH_COLLECTION
// and populates the final packed vec directly -- no temporary zend_array. The observable
// contract (left-to-right evaluation, validate-only-after-all-elements-run, exact cleanup on
// every failure path) is identical to the former array-backed pipeline.

function ev(string $tag, $v) { echo "eval $tag\n"; return $v; }

echo "-- values, order, exact size --\n";
$v = vec[int]{ ev('0', 10), ev('1', 20), ev('2', 30) };
var_dump($v->count, $v[0], $v[2]);

echo "-- empty --\n";
var_dump((vec[int]{})->count);

echo "-- nested vec (inner literals are themselves direct builders) --\n";
$n = vec[vec[int]]{ vec[int]{1, 2}, vec[int]{3} };
var_dump($n[0][1], $n[1][0], $n->count);

echo "-- refcounted elements: strings (TMP move) and objects (identity kept) --\n";
function s(int $i): string { return "v$i"; }
$vs = vec[string]{ s(1), s(2), 'lit' };
var_dump($vs[0], $vs[2]);
$o = new stdClass; $o->n = 7;
$vo = vec[stdClass]{ $o };
var_dump($vo[0] === $o);

echo "-- evaluate ALL elements, THEN validate: a wrong element 0 still runs element 1 --\n";
try { $x = vec[int]{ ev('A=wrong', 'str'), ev('B=side-effect', 2) }; }
catch (\TypeError $e) { echo $e->getMessage(), "\n"; }

echo "-- late mismatch: every element runs; the error names the source slot --\n";
try { $x = vec[int]{ ev('0', 1), ev('1', 2), ev('2=wrong', 'str'), ev('3=side-effect', 4) }; }
catch (\TypeError $e) { echo $e->getMessage(), "\n"; }

echo "-- a thrown exception mid-list stops later evaluation and frees the partial payload --\n";
function boom(): int { echo "boom\n"; throw new RuntimeException('mid'); }
try { $x = vec[int]{ ev('0', 1), boom(), ev('2', 3) }; }
catch (\RuntimeException $e) { echo 'caught: ', $e->getMessage(), "\n"; }

echo "-- non-constructible type: elements evaluate first, then 'Cannot create' --\n";
try { $x = vec[?int]{ ev('0', 1), ev('1', 2) }; }
catch (\TypeError $e) { echo $e->getMessage(), "\n"; }

echo "-- value identity still holds for directly-built vecs --\n";
var_dump(vec[int]{1, 2, 3} === vec[int]{1, 2, 3});
var_dump(vec[int]{1, 2, 3} === vec[int]{3, 2, 1});

echo "-- tuple and set are unaffected (still array-backed) --\n";
var_dump((tuple[int, string]{1, 'a'})->count);
var_dump((set[int]{1, 1, 2})->count);
?>
--EXPECT--
-- values, order, exact size --
eval 0
eval 1
eval 2
int(3)
int(10)
int(30)
-- empty --
int(0)
-- nested vec (inner literals are themselves direct builders) --
int(2)
int(3)
int(2)
-- refcounted elements: strings (TMP move) and objects (identity kept) --
string(2) "v1"
string(3) "lit"
bool(true)
-- evaluate ALL elements, THEN validate: a wrong element 0 still runs element 1 --
eval A=wrong
eval B=side-effect
Element 0 of vec[int] must be of type int, string given
-- late mismatch: every element runs; the error names the source slot --
eval 0
eval 1
eval 2=wrong
eval 3=side-effect
Element 2 of vec[int] must be of type int, string given
-- a thrown exception mid-list stops later evaluation and frees the partial payload --
eval 0
boom
caught: mid
-- non-constructible type: elements evaluate first, then 'Cannot create' --
eval 0
eval 1
Cannot create a value of type vec[?int]
-- value identity still holds for directly-built vecs --
bool(true)
bool(false)
-- tuple and set are unaffected (still array-backed) --
int(2)
int(2)

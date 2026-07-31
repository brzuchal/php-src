--TEST--
set dedup: a throw from recursive strict array comparison during the swap-partition is safe
--EXTENSIONS--
zend_test
--FILE--
<?php
// The set builder deduplicates in place at FINISH by comparing slots with zend_is_identical().
// For array members that recurses through zend_hash_compare(), which THROWS a catchable Error
// ("Nesting level too deep - recursive dependency?") when it meets a cyclic array (distinct
// arrays only -- identical pointers short-circuit before recursing). This is the one place the
// dedup partition can throw: it invokes no userland and frees nothing while comparing, but the
// comparison itself may raise.
//
// The partition stops the instant it throws, with `count` unchanged so it still covers every
// initialized slot; a swap only exchanges two owned slots, so ownership stays bijective and the
// payload is a valid owned TMP with no stale alias. FINISH then frees the whole payload and
// propagates the error -- promptly and identically under VM, opcache, function JIT and tracing
// JIT -- so a plain try/catch at the construction site observes it, and every slot is destroyed
// exactly once on the unwind. The error's class and message match the former array-backed path.

function mk(): array { $a = []; $a[] =& $a; return $a; }        // a fresh, distinct cyclic array
function ev(string $t, $v) { echo "eval $t\n"; return $v; }

echo "-- identical pointer twice: short-circuits, dedups to 1, no recursion, no throw --\n";
$a = mk();
var_dump((set[array]{$a, $a})->count);

echo "-- two distinct cyclic arrays: the dedup comparison recurses and throws --\n";
try { $x = set[array]{mk(), mk()}; echo "no throw\n"; }
catch (\Error $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }

echo "-- every element evaluates BEFORE dedup, so the throw is post-evaluation (no later element) --\n";
try { $x = set[array]{ ev('P', mk()), ev('Q', mk()), ev('R', [9]) }; echo "no throw\n"; }
catch (\Error $e) { echo "caught: ", $e->getMessage(), "\n"; }

echo "-- partition has already progressed ([1],[1] dedup, [2] kept) before the throwing compare --\n";
try { $x = set[array]{ [1], [1], [2], mk(), mk() }; echo "no throw\n"; }
catch (\Error $e) { echo "caught: ", $e->getMessage(), "\n"; }

echo "-- execution continues normally after the caught error --\n";
echo "continued\n";

echo "-- repeated stress: the payload must unwind with no leak / UAF / double-free (see ASAN) --\n";
$err = 0;
for ($i = 0; $i < 1000; $i++) {
    try { $z = set[array]{ [1], [1], [$i], mk(), mk() }; }
    catch (\Error $e) { $err++; }
}
echo "stress threw = $err\n";

echo "-- the engine is healthy afterwards --\n";
var_dump((set[int]{1, 1, 2, 3, 3})->count);
echo "OK\n";
?>
--EXPECT--
-- identical pointer twice: short-circuits, dedups to 1, no recursion, no throw --
int(1)
-- two distinct cyclic arrays: the dedup comparison recurses and throws --
Error: Nesting level too deep - recursive dependency?
-- every element evaluates BEFORE dedup, so the throw is post-evaluation (no later element) --
eval P
eval Q
eval R
caught: Nesting level too deep - recursive dependency?
-- partition has already progressed ([1],[1] dedup, [2] kept) before the throwing compare --
caught: Nesting level too deep - recursive dependency?
-- execution continues normally after the caught error --
continued
-- repeated stress: the payload must unwind with no leak / UAF / double-free (see ASAN) --
stress threw = 1000
-- the engine is healthy afterwards --
int(3)
OK

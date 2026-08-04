--TEST--
unserialize(): a set of two distinct recursive arrays throws the recursion Error promptly (no deferred exception)
--EXTENSIONS--
zend_test
--FILE--
<?php
// Regression coverage for the legacy array-backed rebuild path used by unserialize():
// php_var_unserialize_collection() -> zend_collection_construct() -> zend_set_create(), which
// deduplicates set elements with zend_is_identical(). For two DISTINCT cyclic arrays that recurses
// through zend_hash_compare() and throws a catchable Error("Nesting level too deep - recursive
// dependency?") -- the array-path analogue of the FINISH_COLLECTION dedup throw fixed in the direct
// builder. Identical array *pointers* short-circuit before recursing, so the two elements must be
// distinct cyclic arrays.
//
// This pins that the recursion Error propagates PROMPTLY out of unserialize() (unlike the VM-opcode
// path, unserialize() is a userland call whose DO_FCALL exception check surfaces it immediately):
// catchable by a plain try/catch around the call, the assignment never completes (no partial
// collection is observable), execution continues with no deferred exception at the next call or
// opcode, and repeated teardown stays clean (validated separately under debug+ASAN).

// Build the wire form of set[array]{$p, $q} for two distinct self-referential arrays. vec[array]
// does not deduplicate, so it serializes without recursing; rewriting the kind yields the set
// fixture whose rebuild triggers the dedup recursion.
$p = []; $p[] =& $p;
$q = []; $q[] =& $q;
$fixture = preg_replace('/^L:vec:/', 'L:set:', serialize(vec[array]{$p, $q}));

echo "-- the recursion Error is thrown from unserialize() and caught at the call site --\n";
$after = 'SENTINEL';
try {
    $after = unserialize($fixture);
    echo "NOT REJECTED\n";
} catch (\Error $e) {
    echo get_class($e), ": ", $e->getMessage(), "\n";
}

echo "-- the assignment never completed: no partial collection is observable --\n";
var_dump($after);   // still 'SENTINEL'

echo "-- execution continues; no deferred exception at the next call or opcode --\n";
strlen("checkpoint");   // a function call: a deferred exception would surface here
$x = 40 + 2;            // and an arithmetic opcode
echo "continued: $x\n";

echo "-- repeated rebuild+throw stays clean (each throw frees the partial payload) --\n";
$threw = 0;
for ($i = 0; $i < 500; $i++) {
    try { unserialize($fixture); } catch (\Error $e) { $threw++; }
}
echo "threw $threw / 500\n";

echo "-- the engine is healthy afterwards: an ordinary collection round-trips --\n";
var_dump(unserialize(serialize(vec[int]{1, 2, 3}))->count);

echo "-- a non-recursive set fixture still rebuilds and deduplicates normally --\n";
$ok = preg_replace('/^L:vec:/', 'L:set:', serialize(vec[int]{5, 5, 7}));
var_dump(unserialize($ok)->count);   // 2 (5 deduped)
echo "OK\n";
?>
--EXPECT--
-- the recursion Error is thrown from unserialize() and caught at the call site --
Error: Nesting level too deep - recursive dependency?
-- the assignment never completed: no partial collection is observable --
string(8) "SENTINEL"
-- execution continues; no deferred exception at the next call or opcode --
continued: 42
-- repeated rebuild+throw stays clean (each throw frees the partial payload) --
threw 500 / 500
-- the engine is healthy afterwards: an ordinary collection round-trips --
int(3)
-- a non-recursive set fixture still rebuilds and deduplicates normally --
int(2)
OK

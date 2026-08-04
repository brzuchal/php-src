--TEST--
collection DIM on an open-world container under the function JIT (no MAY_BE_COLLECTION inferred)
--EXTENSIONS--
opcache
--INI--
opcache.enable=1
opcache.enable_cli=1
opcache.jit_buffer_size=64M
opcache.jit=function
--FILE--
<?php
// An untyped parameter is inferred without MAY_BE_COLLECTION (the open-world case), so
// the function JIT compiles these DIM ops with its array fast path plus a generic
// non-array branch. A collection arriving at runtime must take VM semantics there:
// reads, coalesce, isset and list must yield elements (formerly a "Trying to access
// array offset" warning and null), and writable fetches must throw the
// immutable-collection Error.
function readAt($c, int $i) { return $c[$i]; }
function readAtK($c, $k) { return $c[$k]; }
function sumAll($c, int $n) { $s = 0; for ($i = 0; $i < $n; $i++) { $s += $c[$i]; } return $s; }
function coal($c, int $i) { return $c[$i] ?? "default"; }
function iss($c, int $i) { return isset($c[$i]); }
function takeTwo($c) { [$a, $b] = $c; return $a . "/" . $b; }
function inc($c) { $c[0]++; }
function byRef($c) { $r = &$c[1]; }
function readConstStr($c) { return $c["0"]; }

$v = vec[int]{10, 20, 30};
$t = tuple[int, string]{7, "x"};
$s = set[int]{1, 2};

// the same compiled bodies serve plain arrays (fast path) and collections
var_dump(sumAll([1, 2, 3], 3), sumAll($v, 3));
var_dump(readAt($v, 0), readAt($t, 1), readAt(["a", "b"], 1));
var_dump(coal($v, 1), coal($v, 9), coal([5], 0));
var_dump(iss($v, 2), iss($v, 3), iss($t, 0));
var_dump(takeTwo($v), takeTwo(["l", "r"]));

function e(string $l, callable $fn): void {
    try { $fn(); echo "$l: NOT REJECTED\n"; }
    catch (\Throwable $ex) { echo "$l: ", get_class($ex), ": ", $ex->getMessage(), "\n"; }
}
e('numeric-string key', function () use ($v) { readAtK($v, "0"); });
e('const "0" key',      function () use ($v) { readConstStr($v); });
e('string key',         function () use ($v) { readAtK($v, "nope"); });
e('out of range',       function () use ($v) { readAt($v, 9); });
e('set index',          function () use ($s) { readAt($s, 0); });
e('inc',                function () use ($v) { inc($v); });
e('by-ref',             function () use ($v) { byRef($v); });

// receivers unchanged after everything above
var_dump($v[0] === 10, $v->count === 3, $t->count === 2);
echo "OK\n";
?>
--EXPECT--
int(6)
int(60)
int(10)
string(1) "x"
string(1) "b"
int(20)
string(7) "default"
int(5)
bool(true)
bool(false)
bool(true)
string(5) "10/20"
string(3) "l/r"
numeric-string key: TypeError: Collection index must be of type int, string given
const "0" key: TypeError: Collection index must be of type int, string given
string key: TypeError: Collection index must be of type int, string given
out of range: ValueError: Collection index 9 is out of range
set index: Error: Cannot index a set
inc: Error: Cannot modify an immutable collection
by-ref: Error: Cannot modify an immutable collection
bool(true)
bool(true)
bool(true)
OK

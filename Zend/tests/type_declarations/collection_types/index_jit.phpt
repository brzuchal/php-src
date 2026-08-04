--TEST--
vec/tuple indexed read under JIT: collection DIM falls back to the VM (function + tracing)
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
// A vec[int]/tuple param is not inferred as a pure array, so the JIT's array-only DIM
// fast path never fires: the collection read runs the VM handler. Values, isset, and the
// out-of-range ValueError must all be correct through the hot path.
function readAt(vec[int] $v, int $i): int { return $v[$i]; }
function issAt(vec[int] $v, int $i): bool { return isset($v[$i]); }

$v = vec[int]{10, 20, 30};
$t = tuple[int, string]{7, "x"};

$sum = 0;
for ($n = 0; $n < 100000; $n++) {
    $sum += readAt($v, $n % 3);
    $sum += issAt($v, $n % 5) ? 1 : 0;
}
var_dump($sum);                       // deterministic

// correctness after warm-up
var_dump(readAt($v, 0) === 10, readAt($v, 2) === 30, $t[1] === "x");
var_dump(issAt($v, 1) === true, issAt($v, 9) === false);
try { readAt($v, 99); } catch (\ValueError $e) { echo $e->getMessage(), "\n"; }
var_dump($v->count === 3);
echo "OK\n";
?>
--EXPECT--
int(2059990)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
Collection index 99 is out of range
bool(true)
OK

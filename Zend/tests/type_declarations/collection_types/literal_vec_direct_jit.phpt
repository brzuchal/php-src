--TEST--
vec direct construction under JIT: the builder opcodes fall back to the VM (function + tracing)
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
// The builder opcodes (INIT_COLLECTION / ADD_COLLECTION_ELEMENT / FINISH_COLLECTION) have no
// native JIT path, so a function/trace containing them runs the VM handlers. Values, order and
// the failure paths must be identical to the interpreter through the hot loop.
function build(int $a, int $b, int $c): int {
    $v = vec[int]{$a, $b, $c};
    return $v[0] + $v[1] + $v[2];
}
function buildStr(int $i): string {
    $v = vec[string]{"a$i", "b$i"};
    return $v[0] . $v[1];
}

$sum = 0;
for ($n = 0; $n < 200000; $n++) {
    $sum += build($n % 3, $n % 5, $n % 7);
    if ($n % 10000 === 0) { $sum += strlen(buildStr($n)); }
}
var_dump($sum);

// correctness + failure paths after warm-up
var_dump(vec[int]{1, 2, 3} === vec[int]{1, 2, 3});
try { $x = vec[int]{1, "bad", 3}; } catch (\TypeError $e) { echo $e->getMessage(), "\n"; }
try { $x = vec[int]{1, (function(){ throw new RuntimeException("x"); })()}; }
catch (\RuntimeException $e) { echo "caught\n"; }
var_dump((vec[vec[int]]{vec[int]{1}, vec[int]{2, 3}})[1]->count);
echo "OK\n";
?>
--EXPECT--
int(1200245)
bool(true)
Element 1 of vec[int] must be of type int, string given
caught
int(2)
OK

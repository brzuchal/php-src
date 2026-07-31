--TEST--
set direct construction under JIT: the builder opcodes fall back to the VM (function + tracing)
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
// set reuses the vec builder opcodes (INIT/ADD/FINISH_COLLECTION); they have no native JIT
// path, so the sequence runs the VM handlers. FINISH additionally deduplicates in place. The
// dedup result, first-occurrence order and failure paths must be identical to the interpreter
// through the hot loop.
function build(int $a): int {
    // three elements evaluated, two distinct: {$a, $a+1}; count is 2
    $s = set[int]{$a, $a, $a + 1, $a};
    $sum = 0;
    foreach ($s as $x) { $sum += $x; }
    return $sum + $s->count;
}
$sum = 0;
for ($n = 0; $n < 200000; $n++) {
    $sum += build($n % 3);
}
var_dump($sum);

// first-occurrence order survives the hot path
function ordered(): string {
    $s = set[int]{4, 2, 4, 1, 2, 3, 1};
    $o = [];
    foreach ($s as $x) { $o[] = $x; }
    return implode(',', $o);
}
for ($n = 0; $n < 100000; $n++) { $r = ordered(); }
var_dump($r);

var_dump(set[int]{1, 1, 2} === set[int]{2, 1});
try { $x = set[int]{1, 1, 'bad'}; } catch (\TypeError $e) { echo $e->getMessage(), "\n"; }
try { $x = set[int]{1, (function(){ throw new RuntimeException("x"); })(), 1}; }
catch (\RuntimeException $e) { echo "caught\n"; }
var_dump((set[vec[int]]{ vec[int]{1, 2}, vec[int]{1, 2}, vec[int]{3} })->count);
echo "OK\n";
?>
--EXPECT--
int(999998)
string(7) "4,2,1,3"
bool(true)
Element 2 of set[int] must be of type int, string given
caught
int(2)
OK

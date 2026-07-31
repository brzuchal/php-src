--TEST--
tuple direct construction under JIT: the builder opcodes fall back to the VM (function + tracing)
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
// tuple reuses the vec builder opcodes; they have no native JIT path, so the sequence runs
// the VM handlers. Positional validation, order and failure paths must be identical to the
// interpreter through the hot loop.
function build(int $a, string $b, float $c): float {
    $t = tuple[int, string, float]{$a, $b, $c};
    return $t[0] + strlen($t[1]) + $t[2];
}
$sum = 0.0;
for ($n = 0; $n < 200000; $n++) {
    $sum += build($n % 3, "x", 0.5);
}
var_dump($sum);
var_dump(tuple[int, string]{1, 'a'} === tuple[int, string]{1, 'a'});
try { $x = tuple[int, string, int]{1, 'a', 'bad'}; } catch (\TypeError $e) { echo $e->getMessage(), "\n"; }
try { $x = tuple[int, int]{1, (function(){ throw new RuntimeException("x"); })()}; }
catch (\RuntimeException $e) { echo "caught\n"; }
var_dump((tuple[vec[int], int]{vec[int]{1, 2, 3}, 9})[0]->count);
echo "OK\n";
?>
--EXPECT--
float(499999)
bool(true)
Element 2 of tuple[int,string,int] must be of type int, string given
caught
int(3)
OK

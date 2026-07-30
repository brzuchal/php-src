--TEST--
vec: withAt()/withoutAt() first-class callables retain the receiver; each call starts fresh
--FILE--
<?php
function fmt(iterable $v): string {
    $out = [];
    foreach ($v as $x) {
        $out[] = var_export($x, true);
    }
    return "[" . implode(", ", $out) . "]";
}

$v = vec[int]{10, 20, 30};
$wa = $v->withAt(...);
$wo = $v->withoutAt(...);
unset($v);                       // receiver survives only via the closures

echo fmt($wa(1, 99)), "\n";      // [10, 99, 30]
echo fmt($wa(1, 88)), "\n";      // [10, 88, 30] — retained receiver not mutated
echo fmt($wo(0)), "\n";          // [20, 30]
echo fmt($wo(2)), "\n";          // [10, 20] — still the original receiver

// clone retains the receiver independently
$wa2 = clone $wa;
unset($wa);
echo fmt($wa2(2, 7)), "\n";      // [10, 20, 7]

// binding is rejected on a collection intrinsic FCC
try { $wo->bindTo(new stdClass); } catch (\Error $e) { echo $e->getMessage(), "\n"; }

// destroyed without invocation must not leak (checked by debug/ASAN)
$v2 = vec[int]{9, 8};
$g = $v2->withoutAt(...);
unset($g, $v2);
echo "done\n";
?>
--EXPECT--
[10, 99, 30]
[10, 88, 30]
[20, 30]
[10, 20]
[10, 20, 7]
Cannot bind a native-collection intrinsic method
done

--TEST--
set: union()/intersect()/diff() first-class callables retain the receiver; binding rejected
--FILE--
<?php
function fmt(iterable $v): string {
    $out = [];
    foreach ($v as $x) {
        $out[] = var_export($x, true);
    }
    return "[" . implode(", ", $out) . "]";
}

$a = set[int]{1, 2, 3};
$u = $a->union(...);
$i = $a->intersect(...);
$d = $a->diff(...);
unset($a);                          // receiver survives only via the closures

echo fmt($u(set[int]{3, 4})), "\n"; // [1, 2, 3, 4]
echo fmt($u(set[int]{5})), "\n";    // [1, 2, 3, 5] — retained receiver not mutated
echo fmt($i(set[int]{2, 3, 9})), "\n"; // [2, 3]
echo fmt($d(set[int]{1})), "\n";    // [2, 3]

// no-op through an FCC returns the retained receiver value
echo fmt($u(set[int]{1, 2})), "\n"; // [1, 2, 3] (subset)
echo fmt($d(set[int]{7})), "\n";    // [1, 2, 3] (disjoint)

// clone retains the receiver independently
$u2 = clone $u;
unset($u);
echo fmt($u2(set[int]{8})), "\n";   // [1, 2, 3, 8]

// binding is rejected on a collection intrinsic FCC (bindTo / bind / call)
try { $d->bindTo(new stdClass); } catch (\Error $e) { echo "bindTo: ", $e->getMessage(), "\n"; }
try { Closure::bind($d, new stdClass); } catch (\Error $e) { echo "bind: ", $e->getMessage(), "\n"; }
try { $d->call(new stdClass, set[int]{1}); } catch (\Error $e) { echo "call: ", $e->getMessage(), "\n"; }

// destroyed without invocation must not leak (checked by debug/ASAN)
$a2 = set[int]{9};
$g = $a2->union(...);
unset($g, $a2);
echo "done\n";
?>
--EXPECT--
[1, 2, 3, 4]
[1, 2, 3, 5]
[2, 3]
[2, 3]
[1, 2, 3]
[1, 2, 3]
[1, 2, 3, 8]
bindTo: Cannot bind a native-collection intrinsic method
bind: Cannot bind a native-collection intrinsic method
call: Cannot bind a native-collection intrinsic method
done

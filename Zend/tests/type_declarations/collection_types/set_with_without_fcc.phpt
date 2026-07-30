--TEST--
set: with()/without() first-class callables retain the receiver; each call starts from the original
--FILE--
<?php
function fmt(iterable $v): string {
    $out = [];
    foreach ($v as $x) {
        $out[] = var_export($x, true);
    }
    return "[" . implode(", ", $out) . "]";
}

$s = set[int]{1, 2};
$w = $s->with(...);
$wo = $s->without(...);
unset($s);                       // receiver survives only via the closures

echo fmt($w(3)), "\n";           // [1, 2, 3]
echo fmt($w(4)), "\n";           // [1, 2, 4] — retained receiver not mutated
echo fmt($wo(1)), "\n";          // [2]
echo fmt($wo(2)), "\n";          // [1] — still the original receiver

// no-op through an FCC returns the retained receiver value
echo fmt($w(2)), "\n";           // [1, 2] (2 already present)
echo fmt($wo(99)), "\n";         // [1, 2] (99 absent)

// clone retains the receiver independently
$w2 = clone $w;
unset($w);
echo fmt($w2(5)), "\n";          // [1, 2, 5]

// binding is rejected on a collection intrinsic FCC (bindTo / bind / call)
try { $wo->bindTo(new stdClass); } catch (\Error $e) { echo "bindTo: ", $e->getMessage(), "\n"; }
try { Closure::bind($wo, new stdClass); } catch (\Error $e) { echo "bind: ", $e->getMessage(), "\n"; }
try { $wo->call(new stdClass, 1); } catch (\Error $e) { echo "call: ", $e->getMessage(), "\n"; }

// destroyed without invocation must not leak (checked by debug/ASAN)
$s2 = set[int]{9};
$g = $s2->with(...);
unset($g, $s2);
echo "done\n";
?>
--EXPECT--
[1, 2, 3]
[1, 2, 4]
[2]
[1]
[1, 2]
[1, 2]
[1, 2, 5]
bindTo: Cannot bind a native-collection intrinsic method
bind: Cannot bind a native-collection intrinsic method
call: Cannot bind a native-collection intrinsic method
done

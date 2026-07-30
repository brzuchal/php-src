--TEST--
tuple: withAt() first-class callable retains the receiver; each call starts from the original
--FILE--
<?php
function fmt(iterable $v): string {
    $out = [];
    foreach ($v as $x) {
        $out[] = var_export($x, true);
    }
    return "[" . implode(", ", $out) . "]";
}

$t = tuple[int, string]{1, "a"};
$f = $t->withAt(...);
unset($t);                       // receiver survives only via the closure

echo fmt($f(0, 9)), "\n";        // [9, 'a']
echo fmt($f(1, "b")), "\n";      // [1, 'b'] — retained receiver not mutated
echo fmt($f(0, 5)), "\n";        // [5, 'a'] — every call starts from the original

// chaining on the result
echo fmt($f(0, 9)->withAt(1, "z")), "\n";  // [9, 'z']

// clone retains the receiver independently
$c = clone $f;
unset($f);
echo fmt($c(1, "cl")), "\n";     // [1, 'cl']

// binding is rejected on a collection intrinsic FCC (bindTo / bind / call)
try { $c->bindTo(new stdClass); } catch (\Error $e) { echo "bindTo: ", $e->getMessage(), "\n"; }
try { Closure::bind($c, new stdClass); } catch (\Error $e) { echo "bind: ", $e->getMessage(), "\n"; }
try { $c->call(new stdClass, 0, 1); } catch (\Error $e) { echo "call: ", $e->getMessage(), "\n"; }

// destroyed without invocation must not leak (checked by debug/ASAN)
$t2 = tuple[int, string]{7, "q"};
$g = $t2->withAt(...);
unset($g, $t2);
echo "done\n";
?>
--EXPECT--
[9, 'a']
[1, 'b']
[5, 'a']
[9, 'z']
[1, 'cl']
bindTo: Cannot bind a native-collection intrinsic method
bind: Cannot bind a native-collection intrinsic method
call: Cannot bind a native-collection intrinsic method
done

--TEST--
tuple: withAt() basic semantics (immutable, positional, exact descriptor, arity invariant)
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

// replace at first / last position
echo "withAt 0: ", fmt($t->withAt(0, 2)), "\n";
echo "withAt 1: ", fmt($t->withAt(1, "b")), "\n";

// receiver unchanged; arity invariant
echo "receiver: ", fmt($t), " count=", $t->count, "\n";

// heterogeneous 3-tuple: first / middle / last, each keeps its position's type
$h = tuple[int, string, bool]{1, "x", true};
echo "3 first:  ", fmt($h->withAt(0, 9)), "\n";
echo "3 middle: ", fmt($h->withAt(1, "y")), "\n";
echo "3 last:   ", fmt($h->withAt(2, false)), "\n";

// one-element tuple
$o = tuple[int]{7};
echo "one: ", fmt($o->withAt(0, 9)), "\n";

// exact descriptor preserved: a tuple[int, string] result still dumps as such
var_dump($t->withAt(0, 2));

// temporary receiver returned from a function
function make(): tuple[int, string] { return tuple[int, string]{5, "m"}; }
echo "temp: ", fmt(make()->withAt(1, "n")), "\n";

// chaining: arity stays 2, each step is a new value
echo "chain: ", fmt($t->withAt(0, 3)->withAt(1, "c")), "\n";
?>
--EXPECT--
withAt 0: [2, 'a']
withAt 1: [1, 'b']
receiver: [1, 'a'] count=2
3 first:  [9, 'x', true]
3 middle: [1, 'y', true]
3 last:   [1, 'x', false]
one: [9]
tuple[int,string](2) {
  [0]=>
  int(2)
  [1]=>
  string(1) "a"
}
temp: [5, 'n']
chain: [3, 'c']

--TEST--
vec: withAt() and withoutAt() basic semantics (immutable, exact descriptor)
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

// withAt at first / middle / last index
echo "withAt 0: ", fmt($v->withAt(0, 99)), "\n";
echo "withAt 1: ", fmt($v->withAt(1, 99)), "\n";
echo "withAt 2: ", fmt($v->withAt(2, 99)), "\n";

// withoutAt at first / middle / last index (subsequent indexes compacted)
echo "withoutAt 0: ", fmt($v->withoutAt(0)), "\n";
echo "withoutAt 1: ", fmt($v->withoutAt(1)), "\n";
echo "withoutAt 2: ", fmt($v->withoutAt(2)), "\n";

// receiver is never mutated
echo "receiver: ", fmt($v), " count=", $v->count, "\n";

// one-element vec
$one = vec[int]{5};
echo "one withAt: ", fmt($one->withAt(0, 9)), "\n";

// removing the only element yields an empty vec with the same descriptor
$empty = $one->withoutAt(0);
echo "empty: ", fmt($empty), " count=", $empty->count, "\n";
var_dump($empty);

// exact descriptor preserved: a vec[int] result still dumps as vec[int]
var_dump($v->withAt(1, 99));

// vec[string] preserves its element type
$s = vec[string]{"a", "b", "c"};
echo "string withAt: ", fmt($s->withAt(1, "X")), "\n";
echo "string withoutAt: ", fmt($s->withoutAt(2)), "\n";

// temporary receiver returned from a function
function make(): vec[int] { return vec[int]{7, 8, 9}; }
echo "temp withAt: ", fmt(make()->withAt(0, 0)), "\n";
echo "temp withoutAt: ", fmt(make()->withoutAt(1)), "\n";

// chaining: each step is a new value
echo "chain: ", fmt($v->withAt(0, 1)->withoutAt(2)->withAt(1, 2)), "\n";
?>
--EXPECT--
withAt 0: [99, 20, 30]
withAt 1: [10, 99, 30]
withAt 2: [10, 20, 99]
withoutAt 0: [20, 30]
withoutAt 1: [10, 30]
withoutAt 2: [10, 20]
receiver: [10, 20, 30] count=3
one withAt: [9]
empty: [] count=0
vec[int](0) {
}
vec[int](3) {
  [0]=>
  int(10)
  [1]=>
  int(99)
  [2]=>
  int(30)
}
string withAt: ['a', 'X', 'c']
string withoutAt: ['a', 'b']
temp withAt: [0, 8, 9]
temp withoutAt: [7, 9]
chain: [1, 2]

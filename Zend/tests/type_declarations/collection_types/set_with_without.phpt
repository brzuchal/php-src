--TEST--
set: with() and without() basic semantics (immutable, strict-identity membership, exact descriptor)
--FILE--
<?php
function fmt(iterable $v): string {
    $out = [];
    foreach ($v as $x) {
        $out[] = var_export($x, true);
    }
    return "[" . implode(", ", $out) . "]";
}

// add to empty and non-empty; a new member goes at the end
$e = set[int]{};
echo "empty with(1): ", fmt($e->with(1)), "\n";
$s = set[int]{1, 2, 3};
echo "with(4):       ", fmt($s->with(4)), "\n";

// adding an existing value (first / middle / last) is a no-op: the value is unchanged
echo "with(1) noop:  ", fmt($s->with(1)), "\n";
echo "with(2) noop:  ", fmt($s->with(2)), "\n";
echo "with(3) noop:  ", fmt($s->with(3)), "\n";

// removing first / middle / last compacts the remaining members in order
echo "without(1):    ", fmt($s->without(1)), "\n";
echo "without(2):    ", fmt($s->without(2)), "\n";
echo "without(3):    ", fmt($s->without(3)), "\n";

// removing an absent value is a no-op
echo "without(99):   ", fmt($s->without(99)), "\n";

// receiver never mutated
echo "receiver:      ", fmt($s), " count=", $s->count, "\n";

// removing the only element yields an empty set of the same descriptor
$one = set[int]{7};
$empty = $one->without(7);
echo "remove only:   ", fmt($empty), " count=", $empty->count, "\n";
var_dump($empty);

// exact descriptor preserved
var_dump($s->with(4));

// construction dedup is consistent with with(): repeats collapse to one, order kept
$d = set[int]{1, 1, 2, 3, 2};
echo "ctor dedup:    ", fmt($d), " count=", $d->count, "\n";

// strict identity: distinct scalars of the element type coexist
$b = set[bool]{true};
echo "bool with(false):   ", fmt($b->with(false)), "\n";
$bf = set[bool]{true, false};
echo "bool without(true): ", fmt($bf->without(true)), "\n";
$f = set[float]{1.0, 2.0};
echo "float with(1.0) noop: ", fmt($f->with(1.0)), "\n";
echo "float with(3.5):      ", fmt($f->with(3.5)), "\n";

// temporary receiver
function make(): set[int] { return set[int]{5, 6}; }
echo "temp with(7):   ", fmt(make()->with(7)), "\n";
echo "temp without(5): ", fmt(make()->without(5)), "\n";

// chaining: remove then re-add lands the value at the end
echo "chain: ", fmt($s->with(4)->without(2)->with(2)), "\n";
?>
--EXPECT--
empty with(1): [1]
with(4):       [1, 2, 3, 4]
with(1) noop:  [1, 2, 3]
with(2) noop:  [1, 2, 3]
with(3) noop:  [1, 2, 3]
without(1):    [2, 3]
without(2):    [1, 3]
without(3):    [1, 2]
without(99):   [1, 2, 3]
receiver:      [1, 2, 3] count=3
remove only:   [] count=0
set[int](0) {
}
set[int](4) {
  [0]=>
  int(1)
  [1]=>
  int(2)
  [2]=>
  int(3)
  [3]=>
  int(4)
}
ctor dedup:    [1, 2, 3] count=3
bool with(false):   [true, false]
bool without(true): [false]
float with(1.0) noop: [1.0, 2.0]
float with(3.5):      [1.0, 2.0, 3.5]
temp with(7):   [5, 6, 7]
temp without(5): [6]
chain: [1, 3, 4, 2]

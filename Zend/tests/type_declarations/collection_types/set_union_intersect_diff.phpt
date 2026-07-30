--TEST--
set: union()/intersect()/diff() semantics — overlap, subset, superset, disjoint, ordering
--FILE--
<?php
function fmt(iterable $v): string {
    $out = [];
    foreach ($v as $x) {
        $out[] = var_export($x, true);
    }
    return "[" . implode(", ", $out) . "]";
}
function show(string $l, iterable $u, iterable $i, iterable $d): void {
    echo str_pad($l, 11), " union=", fmt($u), " intersect=", fmt($i), " diff=", fmt($d), "\n";
}

$a = set[int]{1, 2, 3};

// overlap (both directions: order is receiver-driven)
$b = set[int]{3, 4, 5};
show("overlap",   $a->union($b), $a->intersect($b), $a->diff($b));
show("overlap-r", $b->union($a), $b->intersect($a), $b->diff($a));

// subset / superset
$sub = set[int]{1, 2};
$sup = set[int]{1, 2, 3, 4};
show("subset",    $a->union($sub), $a->intersect($sub), $a->diff($sub));
show("superset",  $a->union($sup), $a->intersect($sup), $a->diff($sup));

// disjoint / equal
$dis = set[int]{7, 8};
$eq  = set[int]{1, 2, 3};
show("disjoint",  $a->union($dis), $a->intersect($dis), $a->diff($dis));
show("equal",     $a->union($eq), $a->intersect($eq), $a->diff($eq));

// empty operand (either side)
$e = set[int]{};
show("empty-arg", $a->union($e), $a->intersect($e), $a->diff($e));
show("empty-recv",$e->union($a), $e->intersect($a), $e->diff($a));

// ordering: new union members appear in the OTHER set's order, after all of base
$x = set[int]{1};
$y = set[int]{5, 3, 4};
echo "order: ", fmt($x->union($y)), "\n";

// receiver never mutated
echo "recv: ", fmt($a), " count=", $a->count, "\n";

// exact descriptor preserved; an empty result still carries set[int]
var_dump($a->union($b));
var_dump($a->diff($sup));

// chaining
echo "chain: ", fmt($a->union($b)->diff($sub)->intersect(set[int]{3, 4, 5, 9})), "\n";
?>
--EXPECT--
overlap     union=[1, 2, 3, 4, 5] intersect=[3] diff=[1, 2]
overlap-r   union=[3, 4, 5, 1, 2] intersect=[3] diff=[4, 5]
subset      union=[1, 2, 3] intersect=[1, 2] diff=[3]
superset    union=[1, 2, 3, 4] intersect=[1, 2, 3] diff=[]
disjoint    union=[1, 2, 3, 7, 8] intersect=[] diff=[1, 2, 3]
equal       union=[1, 2, 3] intersect=[1, 2, 3] diff=[]
empty-arg   union=[1, 2, 3] intersect=[] diff=[1, 2, 3]
empty-recv  union=[1, 2, 3] intersect=[] diff=[]
order: [1, 5, 3, 4]
recv: [1, 2, 3] count=3
set[int](5) {
  [0]=>
  int(1)
  [1]=>
  int(2)
  [2]=>
  int(3)
  [3]=>
  int(4)
  [4]=>
  int(5)
}
set[int](0) {
}
chain: [3, 4, 5]

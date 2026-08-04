--TEST--
vec hybrid: hybrid append exclusivity matrix (in-place / grow / copy / new-root) + flatten at the tail bound
--EXTENSIONS--
zend_test
--FILE--
<?php
function s(mixed $v): string { return implode(',', iterator_to_array($v)); }

// (A) Exclusive chain off a retained base: first append is retained (flat->hybrid),
// the rest are exclusive temporaries that grow the tail IN PLACE. The base is shared
// and unchanged.
zend_test_make_vec([0, 1, 2], 'int', $base);
$c = $base->append(3)->append(4)->append(5)->append(6);
echo "A chain: ", s($c), "\n";
echo "A base unchanged: ", s($base), "\n";

// (B) Shared-hybrid branching: $h is retained, so each branch is a new root that
// shares the base and owns an independent tail; $h is never mutated.
$h = $base->append(1);
$a = $h->append(2);
$b = $h->append(3);
echo "B h=", s($h), " a=", s($a), " b=", s($b), "\n";

// (C) A multi-element tail reads correctly across the base|tail boundary.
$t = $base->append(10)->append(11)->append(12);   // base=[0,1,2], tail=[10,11,12]
echo "C t[2]=$t[2] t[3]=$t[3] t[5]=$t[5] all=", s($t), "\n";

// (D) Loop-reassign across the flatten bound (T=127) stays correct.
zend_test_make_vec(range(0, 4), 'int', $b2);
$x = $b2;
for ($i = 5; $i < 520; $i++) $x = $x->append($i);
$xa = iterator_to_array($x);
echo "D loop count=", count($xa), " correct=", ($xa === range(0, 519) ? 'yes' : 'NO'), "\n";

// (E) Alias safety: once a hybrid is retained by an alias it is shared, so a further
// append cannot mutate it in place -- the alias and original stay put.
zend_test_make_vec([1, 2], 'int', $bb);
$orig  = $bb->append(3);
$alias = $orig;
$next  = $orig->append(4);
echo "E orig=", s($orig), " alias=", s($alias), " next=", s($next), "\n";
?>
--EXPECT--
A chain: 0,1,2,3,4,5,6
A base unchanged: 0,1,2
B h=0,1,2,1 a=0,1,2,1,2 b=0,1,2,1,3
C t[2]=2 t[3]=10 t[5]=12 all=0,1,2,10,11,12
D loop count=520 correct=yes
E orig=1,2,3 alias=1,2,3 next=1,2,3,4

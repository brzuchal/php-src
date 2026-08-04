--TEST--
vec hybrid: GC reclaims cycles through a hybrid's tail and base; shared base survives churn
--EXTENSIONS--
zend_test
--FILE--
<?php
class Node {
    public $ref = null;
    public static int $freed = 0;
    public function __destruct() { self::$freed++; }
}

// (1) Cycle through the TAIL element: hybrid -> tail -> obj -> hybrid.
// Empty base so the only Node is the one in the tail; keep $base alive so it is
// not part of the cycle.
zend_test_make_vec([], 'Node', $base);
$obj = new Node();
$h = $base->append($obj);        // hybrid: base=[], tail=[obj]
$obj->ref = $h;                  // close the cycle
$f0 = Node::$freed;
unset($h, $obj);                 // no external reference to the cycle remains
gc_collect_cycles();
printf("tail-cycle collected: %s\n", Node::$freed - $f0 >= 1 ? 'yes' : 'NO');

// (2) Cycle through a BASE element, with the base shared by the hybrid:
// hybrid -> base -> o2 -> hybrid.
$o2 = new Node();
zend_test_make_vec([$o2], 'Node', $base2);
$h2 = $base2->append(new Node());  // hybrid: base=[o2], tail=[n3]
$o2->ref = $h2;                    // close the cycle
$f1 = Node::$freed;
unset($h2, $o2, $base2);           // drop every external reference
gc_collect_cycles();
printf("base-cycle collected: %s\n", Node::$freed - $f1 >= 2 ? 'yes' : 'NO');

// (3) A base shared by many branches must survive a collection and branch churn
// without being freed early or corrupted.
zend_test_make_vec(range(0, 999), 'int', $shared);
$branches = [];
for ($i = 0; $i < 64; $i++) $branches[] = $shared->append($i);
gc_collect_cycles();               // nothing unreachable: must free none of them
unset($branches);                  // release the branches; base rc returns to 1
gc_collect_cycles();
$again = $shared->append(-1);      // base still valid -> a fresh branch still works
unset($again, $shared);
printf("shared-base churn: ok\n");

// (4) Stress: many hybrid create/destroy rounds interleaved with collection.
for ($r = 0; $r < 2000; $r++) {
    zend_test_make_vec([1, 2], 'int', $tmp);
    $x = $tmp->append($r);
    unset($x, $tmp);
    if (($r & 255) === 0) gc_collect_cycles();
}
gc_collect_cycles();
printf("stress churn: ok\n");
?>
--EXPECT--
tail-cycle collected: yes
base-cycle collected: yes
shared-base churn: ok
stress churn: ok

--TEST--
set: union()/intersect()/diff() run correctly inside an observed frame (observer active)
--EXTENSIONS--
zend_test
--INI--
zend_test.observer.enabled=1
zend_test.observer.show_output=1
zend_test.observer.observe_all=1
--FILE--
<?php
// Like the other collection intrinsics, the binary set methods are synthetic
// scope-less functions and are not separately instrumented by the observer. What must
// hold is that they run correctly under the observer's begin/end hooks -- including the
// no-op reuse path -- without disturbing observation: the userland run() frame is still
// observed and the results are correct. No nested internal call, so the trace is
// identical with and without JIT.
function run() {
    $a = set[int]{1, 2, 3};
    $b = set[int]{3, 4, 5};
    $u = $a->union($b);
    $i = $a->intersect($b);
    $d = $a->diff($b);
    echo $u->count, " ", $i->count, " ", $d->count, " ", $a->count, "\n";
}
run();

// FCC created and invoked while the observer is active
$a = set[int]{1, 2};
$f = $a->union(...);
unset($a);
echo $f(set[int]{3})->count, "\n";
echo "DONE\n";
?>
--EXPECTF--
<!-- init '%s' -->
<file '%s'>
  <!-- init run() -->
  <run>
5 1 2 3
  </run>
3
DONE
</file '%s'>

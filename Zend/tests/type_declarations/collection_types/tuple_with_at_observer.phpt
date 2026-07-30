--TEST--
tuple: withAt() runs correctly inside an observed frame (observer active, no crash)
--EXTENSIONS--
zend_test
--INI--
zend_test.observer.enabled=1
zend_test.observer.show_output=1
zend_test.observer.observe_all=1
--FILE--
<?php
// Like the vec intrinsics, tuple::withAt() is a synthetic scope-less function and
// is not separately instrumented by the observer. What must hold is that it runs
// correctly under the observer's begin/end hooks without disturbing observation:
// the userland run() frame that contains it is still observed, and the results
// (and the immutable receiver) are correct. The trace has no nested internal call
// so it is identical with and without JIT.
function run() {
    $t = tuple[int, string]{1, "a"};
    $a = $t->withAt(0, 9);
    $b = $t->withAt(1, "b");
    echo $a->count, " ", $b->count, " ", $t->count, "\n";
}
run();

// FCC created and invoked while the observer is active
$t = tuple[int, string]{5, "x"};
$f = $t->withAt(...);
unset($t);
echo $f(0, 7)->count, "\n";
echo "DONE\n";
?>
--EXPECTF--
<!-- init '%s' -->
<file '%s'>
  <!-- init run() -->
  <run>
2 2 2
  </run>
2
DONE
</file '%s'>

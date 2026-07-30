--TEST--
vec: withAt()/withoutAt() run correctly inside an observed frame (observer active, no crash)
--EXTENSIONS--
zend_test
--INI--
zend_test.observer.enabled=1
zend_test.observer.show_output=1
zend_test.observer.observe_all=1
--FILE--
<?php
// The collection intrinsics are synthetic scope-less functions and, like
// append()/prepend(), are not separately instrumented by the observer. What must
// hold is that they execute correctly under the observer's begin/end hooks
// without disturbing observation: the userland run() frame that contains them is
// still observed, and the results (and the immutable receiver) are correct. The
// trace is deliberately free of nested internal calls so it is identical with and
// without JIT.
function run() {
    $v = vec[int]{10, 20, 30};
    $a = $v->withAt(1, 99);
    $b = $v->withoutAt(0);
    echo $a->count, " ", $b->count, " ", $v->count, "\n";
}
run();

// FCC created and invoked while the observer is active
$v = vec[int]{5, 6};
$f = $v->withAt(...);
unset($v);
echo $f(0, 9)->count, "\n";
echo "DONE\n";
?>
--EXPECTF--
<!-- init '%s' -->
<file '%s'>
  <!-- init run() -->
  <run>
3 2 3
  </run>
2
DONE
</file '%s'>

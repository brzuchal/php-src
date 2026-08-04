--TEST--
set: with()/without() run correctly inside an observed frame (observer active, no crash)
--EXTENSIONS--
zend_test
--INI--
zend_test.observer.enabled=1
zend_test.observer.show_output=1
zend_test.observer.observe_all=1
--FILE--
<?php
// Like the vec/tuple intrinsics, set::with()/without() are synthetic scope-less
// functions and are not separately instrumented by the observer. What must hold is
// that they run correctly under the observer's begin/end hooks -- including the
// no-op path that returns the receiver -- without disturbing observation: the
// userland run() frame that contains them is still observed and the results are
// correct. The trace has no nested internal call so it is identical with and
// without JIT.
function run() {
    $s = set[int]{1, 2};
    $a = $s->with(3);
    $b = $s->without(1);
    $c = $s->with(2);   // no-op
    echo $a->count, " ", $b->count, " ", $c->count, " ", $s->count, "\n";
}
run();

// FCC created and invoked while the observer is active
$s = set[int]{5, 6};
$f = $s->with(...);
unset($s);
echo $f(7)->count, "\n";
echo "DONE\n";
?>
--EXPECTF--
<!-- init '%s' -->
<file '%s'>
  <!-- init run() -->
  <run>
3 1 2 2
  </run>
3
DONE
</file '%s'>

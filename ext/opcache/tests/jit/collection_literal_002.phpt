--TEST--
JIT collection literal: inline construction and destruction inside a traced loop
--INI--
opcache.enable=1
opcache.enable_cli=1
opcache.jit_buffer_size=16M
opcache.jit=tracing
opcache.jit_hot_loop=1
opcache.jit_hot_side_exit=1
opcache.file_update_protection=0
--EXTENSIONS--
opcache
zend_test
--FILE--
<?php

/* Regression: a collection value's zval type (IS_COLLECTION == 21) is not
 * representable in the trace's type machinery. Recorded raw it decodes as
 * IS_TRACE_PACKED|IS_DOUBLE, and it positionally aliases MAY_BE_ARRAY_PACKED.
 * When the literal was built and destroyed *inline* in a traced loop -- unlike
 * collection_literal_001.phpt, which builds inside a callee -- the JIT
 * specialised the value as a double and corrupted the heap. The recorder must
 * record it as unknown so the trace stays generic. */

echo "-- flat vec built and destroyed inline --\n";
for ($i = 0; $i < 1000; $i++) {
    $v = vec[int]{$i, $i + 1};
    if (zend_test_vec_get($v, 0) !== $i || zend_test_vec_get($v, 1) !== $i + 1) {
        echo "wrong element at $i\n";
    }
    unset($v);
}
echo "ok\n";

echo "-- nested vec built and destroyed inline --\n";
$ids = [];
for ($i = 0; $i < 1000; $i++) {
    $v = vec[vec[int]]{ vec[int]{$i}, vec[int]{} };
    $ids[zend_test_vec_type_id($v)] = true;
    unset($v);
}
var_dump(count($ids) === 1);

echo "-- a collection value copied and carried across iterations --\n";
$last = null;
for ($i = 0; $i < 1000; $i++) {
    $v = vec[int]{$i};
    $last = $v;   /* copied out of the loop body: exercises refcount, not just free */
}
var_dump(zend_test_vec_get($last, 0) === 999);

?>
--EXPECT--
-- flat vec built and destroyed inline --
ok
-- nested vec built and destroyed inline --
bool(true)
-- a collection value copied and carried across iterations --
bool(true)

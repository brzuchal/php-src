--TEST--
vec hybrid: zend_test_vec_get reads by logical position on both representations
--EXTENSIONS--
zend_test
--FILE--
<?php
/* FLAT */
zend_test_make_vec([1, 2, 3], 'int', $f);
var_dump(zend_test_vec_get($f, 1));

/* HYBRID: the retained append shares the base [10,20,30] and opens a
 * one-element tail; the second, exclusive append grows the tail in place,
 * so reads cross the base|tail boundary at logical index 3. */
zend_test_make_vec([10, 20, 30], 'int', $b);
$h = $b->append(40)->append(50);
var_dump(zend_test_vec_get($h, 0));   // base first
var_dump(zend_test_vec_get($h, 2));   // base last
var_dump(zend_test_vec_get($h, 3));   // tail first
var_dump(zend_test_vec_get($h, 4));   // tail last
try {
    zend_test_vec_get($h, 5);
} catch (ValueError $e) {
    echo $e->getMessage(), "\n";
}
try {
    zend_test_vec_get($h, -1);
} catch (ValueError $e) {
    echo $e->getMessage(), "\n";
}
?>
--EXPECT--
int(2)
int(10)
int(30)
int(40)
int(50)
zend_test_vec_get(): Argument #2 ($index) is out of range
zend_test_vec_get(): Argument #2 ($index) is out of range

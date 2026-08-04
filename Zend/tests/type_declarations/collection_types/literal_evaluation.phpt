--TEST--
collection literals: elements are ordinary expressions, evaluated left to right
--EXTENSIONS--
zend_test
--FILE--
<?php

function e(string $tag, $value) { echo "eval $tag\n"; return $value; }

echo "-- left to right --\n";
$v = vec[int]{ e('a', 1), e('b', 2), e('c', 3) };
var_dump(zend_test_vec_count($v), zend_test_vec_get($v, 2));

echo "-- arbitrary expressions --\n";
$n = 3;
$w = vec[int]{ $n, $n * 2, strlen('abcd'), (int) '5', $n <=> 1 };
$seen = [];
for ($i = 0; $i < zend_test_vec_count($w); $i++) {
    $seen[] = zend_test_vec_get($w, $i);
}
echo implode(',', $seen), "\n";

echo "-- nested calls and short circuiting --\n";
$x = vec[string]{ e('outer', strtoupper(e('inner', 'q'))) };
var_dump(zend_test_vec_get($x, 0));

echo "-- an element that throws: earlier side effects stand, nothing escapes --\n";
function boom() { throw new RuntimeException('boom'); }
$before = null;
try {
    $before = vec[int]{ e('1', 1), boom(), e('3', 3) };
} catch (RuntimeException $ex) {
    echo 'caught ', $ex->getMessage(), "\n";
}
var_dump($before);

echo "-- a bad element type: every element is still evaluated first --\n";
try {
    $bad = vec[int]{ e('1', 1), e('2', 'not an int'), e('3', 3) };
} catch (TypeError $ex) {
    echo 'caught ', $ex->getMessage(), "\n";
}

echo "-- the failure leaves no value behind --\n";
$holder = 'untouched';
try {
    $holder = vec[int]{ 1, 'x' };
} catch (TypeError $ex) {
}
var_dump($holder);

echo "-- elements read through variables and references --\n";
$a = 1; $r = &$a;
$refd = vec[int]{ $r, $a };
var_dump(zend_test_vec_count($refd), zend_test_vec_get($refd, 0));

?>
--EXPECT--
-- left to right --
eval a
eval b
eval c
int(3)
int(3)
-- arbitrary expressions --
3,6,4,5,1
-- nested calls and short circuiting --
eval inner
eval outer
string(1) "Q"
-- an element that throws: earlier side effects stand, nothing escapes --
eval 1
caught boom
NULL
-- a bad element type: every element is still evaluated first --
eval 1
eval 2
eval 3
caught Element 1 of vec[int] must be of type int, string given
-- the failure leaves no value behind --
string(9) "untouched"
-- elements read through variables and references --
int(2)
int(1)

--TEST--
collection types: mixed and iterable accept collections; object and array reject them
--EXTENSIONS--
zend_test
--FILE--
<?php
zend_test_make_vec([1, 2], 'int', $v);

function p_mixed(mixed $x): void {}
function r_mixed(mixed $x): mixed { return $x; }
function p_object(object $x): void {}
function p_iterable(iterable $x): void {}
function p_array(array $x): void {}
function p_int(int $x): void {}
function p_vec(vec[int] $x): void {}
function p_vec_str(vec[string] $x): void {}

$check = function (string $label, callable $f): void {
    try { $f(); echo "$label: accepted\n"; }
    catch (TypeError $e) { echo "$label: rejected\n"; }
};

$check('mixed param    ', fn() => p_mixed($v));
$check('mixed return   ', fn() => r_mixed($v));
$check('vec[int] param ', fn() => p_vec($v));
$check('object param   ', fn() => p_object($v));
$check('iterable param ', fn() => p_iterable($v));
$check('array param    ', fn() => p_array($v));
$check('int param      ', fn() => p_int($v));
$check('vec[str] param ', fn() => p_vec_str($v));

class C { public mixed $m; public ?int $i = null; public object $o; }
$c = new C();
$check('mixed property ', function () use ($c, $v) { $c->m = $v; });
$check('?int property  ', function () use ($c, $v) { $c->i = $v; });
$check('object property', function () use ($c, $v) { $c->o = $v; });

// a non-collection value must behave exactly as before
$check('mixed w/ int   ', fn() => p_mixed(1));
$check('object w/ int  ', fn() => p_object(1));
$check('iterable w/ arr', fn() => p_iterable([1]));
?>
--EXPECT--
mixed param    : accepted
mixed return   : accepted
vec[int] param : accepted
object param   : rejected
iterable param : accepted
array param    : rejected
int param      : rejected
vec[str] param : rejected
mixed property : accepted
?int property  : rejected
object property: rejected
mixed w/ int   : accepted
object w/ int  : rejected
iterable w/ arr: accepted
